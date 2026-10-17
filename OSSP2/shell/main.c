/*
 * main.c — ForgeShell entry point (CO-1 shell as a user-space program)
 *
 *  - becomes session leader with the controlling terminal (job control)
 *  - interactive line editing (raw termios: history, arrows, Ctrl+A/E/U/K/W/L)
 *  - runs builtins in-process, external programs via fork/execvp/waitpid
 *  - background jobs, fg/bg, notifications, JSON status snapshot for the UI
 */
#define _GNU_SOURCE
#include <sys/select.h>
#include "shell.h"
#include "parser.h"
#include "jobs.h"
#include "execute.h"
#include "builtins.h"

/* ---- globals expected by the rest of the shell -------------------- */
pid_t  shell_pid  = 0;
pid_t  shell_pgrp = 0;
pid_t  shell_terminal = 0;
int    shell_interactive = 0;
struct termios shell_term_modes;

/* ------------------------------------------------------------------ */
void *xmalloc(size_t n)
{
    void *p = malloc(n);
    if (!p) { perror("malloc"); exit(1); }
    return p;
}

char *xstrdup(const char *s)
{
    char *p = strdup(s ? s : "");
    if (!p) { perror("strdup"); exit(1); }
    return p;
}

/* ================================================================== */
/* Minimal raw-mode line editor                                        */
/* ================================================================== */
#define HISTORY_MAX 200

static char *history[HISTORY_MAX];
static int   history_count = 0;

static void add_history_line(const char *line)
{
    if (history_count > 0 && strcmp(history[history_count - 1], line) == 0)
        return;
    if (history_count == HISTORY_MAX) {
        free(history[0]);
        memmove(history, history + 1, (HISTORY_MAX - 1) * sizeof(char *));
        history_count--;
    }
    history[history_count++] = xstrdup(line);
}

static const char *prompt_str = "";

/* Mask applied by pselect() while waiting for input: empty == nothing
   blocked, so SIGCHLD is delivered atomically with the wait.         */
static sigset_t input_wait_mask;

/*
 * Print every pending job notification and refresh the JSON file.
 * The caller MUST hold SIGCHLD blocked so the table cannot change
 * underneath us; unblocking afterwards makes any pending handler run,
 * which is why callers loop until nothing is left to print.
 */
static void notify_drain(void)
{
    while (jobs_print_notifications() > 0)
        ;
    if (jobs_dirty) { jobs_write_status_file(); jobs_dirty = 0; }
}

/* write(2) whose return value we deliberately ignore (display only) */
static void wr(const char *s, size_t n)
{
    ssize_t r = write(STDOUT_FILENO, s, n);
    (void)r;
}

static void redisplay(char *buf, size_t len, size_t cur)
{
    char seq[64];
    wr("\r\x1b[K", 4);
    wr(prompt_str, strlen(prompt_str));
    if (len) wr(buf, len);
    if (cur < len) {
        int back = (int)(len - cur);
        int n = snprintf(seq, sizeof seq, "\x1b[%dD", back);
        if (n > 0) wr(seq, (size_t)n);
    }
}

/*
 * Read one edited line.  Returns 1 = line ready, 0 = EOF (Ctrl+D on
 * empty line), -1 = interrupted (Ctrl+C).
 *
 * SIGCHLD discipline: the caller blocks SIGCHLD before calling us;
 * we wait with pselect(), which atomically applies the unblocked mask
 * for the duration of the wait.  A child death can therefore never
 * slip between "unblock" and "wait": it either interrupts pselect
 * (EINTR => drain notifications, redraw) or is already pending when
 * pselect starts and interrupts it immediately.  This closes the
 * lost-notification race completely.
 */
static int raw_readline(char *buf, size_t bufsz)
{
    struct termios raw = shell_term_modes;
    raw.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN);
    raw.c_iflag &= ~(IXON | ICRNL | INLCR | IGNCR);
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;
    /* TCSADRAIN: keep typed-ahead input, like bash does */
    tcsetattr(STDIN_FILENO, TCSADRAIN, &raw);

    size_t len = 0, cur = 0;
    int hist_pos = history_count;   /* one past the newest entry */

    redisplay(buf, len, cur);

    for (;;) {
        char c;
        /* SIGCHLD is blocked here (caller's guarantee): safe to touch
           the job table; then atomically unblock + wait with pselect. */
        notify_drain();
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        int pr = pselect(STDIN_FILENO + 1, &rfds, NULL, NULL, NULL,
                         &input_wait_mask);
        if (pr < 0 && errno == EINTR) {
            /* A child changed state while we idle at the prompt:
               the mask is still blocked outside pselect, so we can
               drain notifications and redraw (bash behaviour).      */
            fs_debug("[main] EINTR at prompt\n");
            redisplay(buf, len, cur);
            continue;
        }
        if (pr <= 0) {                        /* error: treat as EOF */
            tcsetattr(STDIN_FILENO, TCSADRAIN, &shell_term_modes);
            return 0;
        }
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n <= 0) {                       /* EOF / error */
            tcsetattr(STDIN_FILENO, TCSADRAIN, &shell_term_modes);
            if (n == 0 && len == 0) return 0;
            if (n == 0) {                   /* Ctrl+D mid-line: ignore */
                continue;
            }
            return 0;
        }

        if (c == '\r' || c == '\n') {
            wr("\n", 1);
            buf[len] = '\0';
            if (len) add_history_line(buf);
            tcsetattr(STDIN_FILENO, TCSADRAIN, &shell_term_modes);
            return 1;
        }
        if (c == 0x03) {                    /* Ctrl+C */
            wr("^C\n", 3);
            /* discard the rest of the aborted line, like bash */
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &shell_term_modes);
            return -1;
        }
        if (c == 0x04) {                    /* Ctrl+D */
            if (len == 0) {
                tcsetattr(STDIN_FILENO, TCSADRAIN, &shell_term_modes);
                return 0;
            }
            continue;                       /* mid-line: ignore */
        }
        if (c == 0x7f || c == 0x08) {       /* backspace */
            if (cur > 0) {
                memmove(buf + cur - 1, buf + cur, len - cur);
                len--; cur--;
                buf[len] = '\0';
                redisplay(buf, len, cur);
            }
            continue;
        }
        if (c == 0x01) { cur = 0;              redisplay(buf, len, cur); continue; } /* ^A */
        if (c == 0x05) { cur = len;            redisplay(buf, len, cur); continue; } /* ^E */
        if (c == 0x0c) { wr("\x1b[2J\x1b[H", 7); redisplay(buf, len, cur); continue; } /* ^L */
        if (c == 0x15) {                      /* ^U kill to start */
            memmove(buf, buf + cur, len - cur);
            len -= cur; cur = 0; buf[len] = '\0';
            redisplay(buf, len, cur);
            continue;
        }
        if (c == 0x0b) {                      /* ^K kill to end */
            len = cur; buf[len] = '\0';
            redisplay(buf, len, cur);
            continue;
        }
        if (c == 0x17) {                      /* ^W delete word before */
            if (cur > 0) {
                size_t s = cur;
                while (s > 0 && buf[s - 1] == ' ') s--;
                while (s > 0 && buf[s - 1] != ' ') s--;
                memmove(buf + s, buf + cur, len - cur);
                len -= (cur - s); cur = s;
                buf[len] = '\0';
                redisplay(buf, len, cur);
            }
            continue;
        }
        if (c == 0x1b) {                      /* escape sequence */
            char seq[3];
            fs_debug("[edit] ESC seen\n");
            if (read(STDIN_FILENO, &seq[0], 1) <= 0) { fs_debug("[edit] seq0 fail\n"); continue; }
            if (seq[0] != '[') { fs_debug("[edit] seq0=%d\n", seq[0]); continue; }
            if (read(STDIN_FILENO, &seq[1], 1) <= 0) { fs_debug("[edit] seq1 fail\n"); continue; }
            fs_debug("[edit] seq1=%d\n", seq[1]);
            if (seq[1] == 'A') {              /* up: older history */
                if (hist_pos > 0) {
                    hist_pos--;
                    snprintf(buf, bufsz, "%s", history[hist_pos]);
                    len = cur = strlen(buf);
                    redisplay(buf, len, cur);
                }
                continue;
            }
            if (seq[1] == 'B') {              /* down: newer history */
                if (hist_pos < history_count) {
                    hist_pos++;
                    if (hist_pos == history_count) buf[0] = '\0';
                    else snprintf(buf, bufsz, "%s", history[hist_pos]);
                    len = cur = strlen(buf);
                    redisplay(buf, len, cur);
                }
                continue;
            }
            if (seq[1] == 'C') {              /* right */
                if (cur < len) { cur++; redisplay(buf, len, cur); }
                continue;
            }
            if (seq[1] == 'D') {              /* left */
                if (cur > 0) { cur--; redisplay(buf, len, cur); }
                continue;
            }
            if (seq[1] == '3') {              /* Delete key */
                char tilde;
                if (read(STDIN_FILENO, &tilde, 1) > 0 && cur < len) {
                    memmove(buf + cur, buf + cur + 1, len - cur - 1);
                    len--; buf[len] = '\0';
                    redisplay(buf, len, cur);
                }
                continue;
            }
            continue;
        }

        /* printable character (also passes UTF-8 bytes through) */
        if ((unsigned char)c >= 0x20 && len + 1 < bufsz) {
            memmove(buf + cur + 1, buf + cur, len - cur);
            buf[cur] = c;
            len++; cur++;
            buf[len] = '\0';
            redisplay(buf, len, cur);
        }
    }
}

/* ================================================================== */
static void make_prompt(char *out, size_t n)
{
    const char *user = getenv("USER");
    if (!user) user = "user";
    char host[128];
    if (gethostname(host, sizeof host) != 0) snprintf(host, sizeof host, "localhost");
    host[sizeof host - 1] = '\0';

    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof cwd)) snprintf(cwd, sizeof cwd, "?");
    const char *home = getenv("HOME");
    char disp[PATH_MAX];
    if (home && *home && strncmp(cwd, home, strlen(home)) == 0 &&
        (cwd[strlen(home)] == '\0' || cwd[strlen(home)] == '/'))
        snprintf(disp, sizeof disp, "~%s", cwd + strlen(home));
    else
        snprintf(disp, sizeof disp, "%s", cwd);

    int stopped = 0;
    for (int i = 0; i < MAX_JOBS; i++)
        if (job_table[i].nspawned && job_table[i].state == JOB_STOPPED) stopped++;

    if (stopped)
        snprintf(out, n, "ForgeShell:%s@%s:%s [%d]$ ", user, host, disp, stopped);
    else
        snprintf(out, n, "ForgeShell:%s@%s:%s$ ", user, host, disp);
}

/* ================================================================== */
static void setup_shell(void)
{
    shell_pid = getpid();
    shell_pgrp = getpgrp();

    shell_interactive = isatty(STDIN_FILENO);

    if (shell_interactive) {
        /* wait until we own the terminal, then become session leader
           with it as the controlling terminal                       */
        while (tcgetpgrp(STDIN_FILENO) != shell_pgrp)
            kill(shell_pid, SIGTTIN);

        if (getsid(0) != shell_pid)
            setsid();
        ioctl(STDIN_FILENO, TIOCSCTTY, 0);

        tcgetattr(STDIN_FILENO, &shell_term_modes);
        job_control_take_terminal();
    } else {
        tcgetattr(STDIN_FILENO, &shell_term_modes);
    }

    /* the shell itself ignores interactive job-control signals; the
       children reset them to SIG_DFL before exec (see execute.c)     */
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = SIG_IGN;
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTSTP, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGTTIN, &sa, NULL);
    sigaction(SIGTTOU, &sa, NULL);
}

/* ================================================================== */
int main(void)
{
    sigemptyset(&input_wait_mask);   /* nothing blocked while waiting */
    setup_shell();
    jobs_init();
    jobs_install_handler();

    if (shell_interactive) {
        printf("ForgeShell %s — mini Linux shell with job control (OSSP)\n",
               SHELL_VERSION);
        printf("Type 'help' for built-in commands, 'jobs' to list background jobs.\n\n");
    }

    char line[MAX_LINE];
    char prompt[PATH_MAX + 1024];
    pipeline_t pl;

    for (;;) {
        /* Block SIGCHLD across the whole prompt/notification section;
           raw_readline unblocks it only inside read().              */
        sigset_t old;
        block_sigchld(&old);
        notify_drain();

        int rc;
        if (shell_interactive) {
            make_prompt(prompt, sizeof prompt);
            prompt_str = prompt;
            rc = raw_readline(line, sizeof line);
            if (rc < 0) continue;             /* interrupted: new prompt */
            if (rc == 0) {                    /* EOF (Ctrl+D or closed) */
                printf("\n");
                break;
            }
        } else {
            /* batch mode: keep SIGCHLD blocked while reading so fgets()
               is never interrupted; reap right after input arrives.   */
            if (!fgets(line, sizeof line, stdin)) {
                unblock_sigchld(&old);
                break;
            }
            unblock_sigchld(&old);
            size_t l = strlen(line);
            while (l && (line[l - 1] == '\n' || line[l - 1] == '\r'))
                line[--l] = '\0';
            if (l == 0 && feof(stdin)) break;
        }

        if (parse_line(line, &pl) < 0) {
            pipeline_free(&pl);
            continue;
        }
        if (pl.ncmds == 0) {                  /* blank line / comment */
            pipeline_free(&pl);
            continue;
        }

        /* run a builtin in the shell's own process (single commands) */
        const builtin_t *b = NULL;
        if (pl.ncmds == 1)
            b = builtin_lookup(pl.cmds[0].argv[0]);

        if (b) {
            int saved[3] = { -1, -1, -1 };
            command_t *c = &pl.cmds[0];
            int redirecting = c->infile || c->outfile || c->errfile ||
                              c->err_to_out;
            if (redirecting && shell_apply_redirections(c, saved) < 0) {
                shell_restore_fds(saved);
                pipeline_free(&pl);
                continue;
            }
            int r = run_builtin(b, c->argv);
            if (saved[0] >= 0) shell_restore_fds(saved);
            pipeline_free(&pl);
            if (r == 1) break;                /* exit builtin */
            continue;
        }

        execute_pipeline(&pl);
        pipeline_free(&pl);
    }

    /* do not leave zombies behind on exit */
    for (int i = 0; i < MAX_JOBS; i++) {
        job_t *j = &job_table[i];
        if (j->nspawned && j->state != JOB_DONE)
            killpg(j->pgid, SIGHUP);
    }
    printf("exit\n");
    return 0;
}
