/*
 * builtins.c — commands the shell runs in its OWN process (CO-1/CO-3)
 */
#define _GNU_SOURCE
#include <ctype.h>
#include "shell.h"
#include "parser.h"
#include "jobs.h"
#include "execute.h"
#include "builtins.h"

/* ------------------------------------------------------------------ */
static int bi_cd(char **argv)
{
    static char oldpwd[PATH_MAX] = "";
    char target[PATH_MAX];

    if (argv[1] == NULL) {
        const char *home = getenv("HOME");
        if (!home) { fprintf(stderr, "cd: HOME not set\n"); return 0; }
        snprintf(target, sizeof target, "%s", home);
    } else if (strcmp(argv[1], "-") == 0) {
        if (oldpwd[0] == '\0') { fprintf(stderr, "cd: OLDPWD not set\n"); return 0; }
        snprintf(target, sizeof target, "%s", oldpwd);
    } else {
        snprintf(target, sizeof target, "%s", argv[1]);
    }

    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof cwd)) cwd[0] = '\0';

    if (chdir(target) != 0) {
        fprintf(stderr, "cd: %s: %s\n", target, strerror(errno));
        return 0;
    }
    snprintf(oldpwd, sizeof oldpwd, "%s", cwd);
    if (strcmp(argv[1] ? argv[1] : "", "-") == 0)
        printf("%s\n", target);
    return 0;
}

static int bi_pwd(char **argv)
{
    (void)argv;
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof cwd)) printf("%s\n", cwd);
    else perror("pwd");
    return 0;
}

static int bi_echo(char **argv)
{
    for (int i = 1; argv[i]; i++)
        printf("%s%s", argv[i], argv[i + 1] ? " " : "");
    printf("\n");
    return 0;
}

static int bi_exit(char **argv)
{
    int code = 0;
    if (argv[1]) code = atoi(argv[1]);
    /* stop any remaining background jobs cleanly */
    for (int i = 0; i < MAX_JOBS; i++) {
        job_t *j = &job_table[i];
        if (!j->nspawned) continue;
        if (j->state != JOB_DONE)
            killpg(j->pgid, SIGHUP);
    }
    exit(code);
}

static int bi_jobs(char **argv)
{
    int longfmt = (argv[1] && strcmp(argv[1], "-l") == 0);
    sigset_t old;
    block_sigchld(&old);
    jobs_print(longfmt);
    unblock_sigchld(&old);
    jobs_dirty = 1;
    return 0;
}

/* Bring a job to the foreground (SIGCONT + terminal + wait). */
static int bi_fg(char **argv)
{
    sigset_t old;
    block_sigchld(&old);

    job_t *j = argv[1] ? job_find_spec(argv[1]) : job_last_stopped();
    if (!j) {
        fprintf(stderr, "fg: no current job\n");
        unblock_sigchld(&old);
        return 0;
    }
    printf("%s\n", j->command);
    j->state     = JOB_RUNNING;
    j->notified  = 1;
    for (int k = 0; k < j->nspawned; k++)
        if (j->pstate[k] == 2) j->pstate[k] = 0;
    j->nstopped = 0;

    killpg(j->pgid, SIGCONT);          /* resume the process group  */
    job_control_give_terminal(j->pgid);
    wait_for_job(j);
    job_control_take_terminal();
    if (j->state == JOB_DONE) job_remove(j);

    unblock_sigchld(&old);
    jobs_write_status_file();
    return 0;
}

/* Resume a stopped job in the background. */
static int bi_bg(char **argv)
{
    sigset_t old;
    block_sigchld(&old);

    job_t *j = argv[1] ? job_find_spec(argv[1]) : job_last_stopped();
    if (!j) {
        fprintf(stderr, "bg: no current job\n");
        unblock_sigchld(&old);
        return 0;
    }
    if (j->state != JOB_STOPPED) {
        fprintf(stderr, "bg: job %%%d is already running\n", j->jid);
        unblock_sigchld(&old);
        return 0;
    }
    j->state    = JOB_RUNNING;
    j->notified = 1;
    for (int k = 0; k < j->nspawned; k++)
        if (j->pstate[k] == 2) j->pstate[k] = 0;
    j->nstopped = 0;
    killpg(j->pgid, SIGCONT);

    printf("[%d]+ %s &\n", j->jid, j->command);
    unblock_sigchld(&old);
    jobs_write_status_file();
    return 0;
}

/* kill [-SIGNAL] {%N | pid} */
static int bi_kill(char **argv)
{
    int i = 1;
    int sig = SIGTERM;

    if (argv[i] && argv[i][0] == '-') {
        const char *s = argv[i] + 1;
        if (strcmp(s, "l") == 0) {
            printf("TERM INT KILL STOP CONT TSTP HUP QUIT\n");
            return 0;
        }
        if (isdigit((unsigned char)*s)) {
            sig = atoi(s);
        } else {
            char name[32];
            size_t n = 0;
            for (const char *c = s; *c && n < sizeof name - 1; c++)
                name[n++] = (char)toupper((unsigned char)*c);
            name[n] = '\0';
            static const struct { const char *n; int sig; } tbl[] = {
                { "TERM", SIGTERM }, { "INT",  SIGINT  }, { "KILL", SIGKILL },
                { "STOP", SIGSTOP }, { "CONT", SIGCONT }, { "TSTP", SIGTSTP },
                { "HUP",  SIGHUP  }, { "QUIT", SIGQUIT },
            };
            int found = 0;
            for (size_t k = 0; k < sizeof tbl / sizeof tbl[0]; k++)
                if (strcmp(tbl[k].n, name) == 0) { sig = tbl[k].sig; found = 1; break; }
            if (!found) {
                fprintf(stderr, "kill: unsupported signal: %s\n", s);
                return 0;
            }
        }
        i++;
    }

    if (!argv[i]) { fprintf(stderr, "kill: usage: kill [-SIG] %%job|pid\n"); return 0; }

    sigset_t old;
    block_sigchld(&old);
    for (; argv[i]; i++) {
        if (argv[i][0] == '%') {
            job_t *j = job_find_spec(argv[i]);
            if (!j) { fprintf(stderr, "kill: %%%s: no such job\n", argv[i] + 1); continue; }
            fs_debug("[kill] %%%s jid=%d pgid=%d sig=%d\n", argv[i] + 1,
                     j->jid, (int)j->pgid, sig);
            if (killpg(j->pgid, sig) < 0) {
                fs_debug("[kill] killpg failed: %s\n", strerror(errno));
                fprintf(stderr, "kill: %%%s: %s\n", argv[i] + 1, strerror(errno));
            }
            if (sig == SIGCONT && j->state == JOB_STOPPED) {
                j->state = JOB_RUNNING;
                for (int k = 0; k < j->nspawned; k++)
                    if (j->pstate[k] == 2) j->pstate[k] = 0;
                j->nstopped = 0;
            }
        } else {
            pid_t pid = (pid_t)atoi(argv[i]);
            if (pid <= 0) { fprintf(stderr, "kill: bad pid: %s\n", argv[i]); continue; }
            if (kill(pid, sig) < 0)
                fprintf(stderr, "kill: %d: %s\n", (int)pid, strerror(errno));
        }
    }
    unblock_sigchld(&old);
    jobs_write_status_file();
    return 0;
}

/* wait for all background children (CO-6). */
static int bi_wait(char **argv)
{
    (void)argv;
    sigset_t old;
    block_sigchld(&old);
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WUNTRACED)) > 0)
        job_update_status(job_find_pid(pid), pid, status);
    unblock_sigchld(&old);
    jobs_write_status_file();
    return 0;
}

static int bi_help(char **argv);

/* ------------------------------------------------------------------ */
static const builtin_t builtins[] = {
    { "cd",   bi_cd,   "cd [dir|-]        change working directory"    },
    { "pwd",  bi_pwd,  "pwd               print working directory"    },
    { "echo", bi_echo, "echo [args...]    print arguments"            },
    { "jobs", bi_jobs, "jobs [-l]         list background jobs"       },
    { "fg",   bi_fg,   "fg [%N]           bring job to foreground"   },
    { "bg",   bi_bg,   "bg  [%N]          resume job in background"  },
    { "kill", bi_kill, "kill [-SIG] %N    send a signal to a job"    },
    { "wait", bi_wait, "wait              wait for background jobs"  },
    { "help", bi_help, "help              this message"               },
    { "exit", bi_exit, "exit [code]       leave the shell"            },
};

static int bi_help(char **argv)
{
    (void)argv;
    printf("ForgeShell %s — built-in commands:\n\n", SHELL_VERSION);
    for (size_t i = 0; i < sizeof builtins / sizeof builtins[0]; i++)
        printf("  %s\n", builtins[i].help);
    printf("\nAlso supported: pipelines (|), redirection (< > >> 2> 2>&1),"
           "\nbackground jobs with '&', env expansion ($HOME, ${PATH}).\n");
    return 0;
}

const builtin_t *builtin_lookup(const char *name)
{
    for (size_t i = 0; i < sizeof builtins / sizeof builtins[0]; i++)
        if (strcmp(builtins[i].name, name) == 0)
            return &builtins[i];
    return NULL;
}

int run_builtin(const builtin_t *b, char **argv)
{
    return b->fn(argv);
}
