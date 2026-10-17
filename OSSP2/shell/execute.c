/*
 * execute.c — fork/exec pipelines with process groups, pipes,
 * redirection, foreground waiting and terminal hand-off (CO-2/CO-3/CO-5)
 */
#define _GNU_SOURCE
#include "shell.h"
#include "parser.h"
#include "jobs.h"
#include "execute.h"

/* ------------------------------------------------------------------ */
void restore_cooked_terminal(void)
{
    if (shell_interactive)
        tcsetattr(STDIN_FILENO, TCSADRAIN, &shell_term_modes);
}

/* Give the controlling terminal to a foreground process group. */
void job_control_give_terminal(pid_t pgid)
{
    if (!shell_interactive) return;
    /* Give the terminal to the child's process group so that Ctrl+C /
       Ctrl+Z from the tty line discipline hit the child, not us.     */
    tcsetpgrp(STDIN_FILENO, pgid);
}

/* Take the terminal back for the shell itself. */
void job_control_take_terminal(void)
{
    if (!shell_interactive) return;
    tcsetpgrp(STDIN_FILENO, shell_pgrp);
}

/* ------------------------------------------------------------------ */
/* Reset everything the shell changed back to default in the child.   */
static void child_reset_signals(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = SIG_DFL;
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTSTP, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGCHLD, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGTTIN, &sa, NULL);
    sigaction(SIGTTOU, &sa, NULL);

    sigset_t set;
    sigemptyset(&set);
    sigprocmask(SIG_SETMASK, &set, NULL);   /* unblock everything */
}

/* Apply <, >, >>, 2>, 2>>, 2>&1 to descriptors (CO-5). */
static int apply_redirections(command_t *c)
{
    if (c->infile) {
        int fd = open(c->infile, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "forgeshell: %s: %s\n", c->infile, strerror(errno));
            return -1;
        }
        if (dup2(fd, STDIN_FILENO) < 0) { perror("dup2"); return -1; }
        close(fd);
    }
    if (c->outfile) {
        int flags = O_WRONLY | O_CREAT | (c->out_append ? O_APPEND : O_TRUNC);
        int fd = open(c->outfile, flags, 0644);
        if (fd < 0) {
            fprintf(stderr, "forgeshell: %s: %s\n", c->outfile, strerror(errno));
            return -1;
        }
        if (dup2(fd, STDOUT_FILENO) < 0) { perror("dup2"); return -1; }
        close(fd);
    }
    if (c->errfile) {
        int flags = O_WRONLY | O_CREAT | (c->err_append ? O_APPEND : O_TRUNC);
        int fd = open(c->errfile, flags, 0644);
        if (fd < 0) {
            fprintf(stderr, "forgeshell: %s: %s\n", c->errfile, strerror(errno));
            return -1;
        }
        if (dup2(fd, STDERR_FILENO) < 0) { perror("dup2"); return -1; }
        close(fd);
    }
    if (c->err_to_out) {
        if (dup2(STDOUT_FILENO, STDERR_FILENO) < 0) { perror("dup2"); return -1; }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/*
 * Block/wait for a foreground job until every child has either exited
 * or stopped (WUNTRACED catches Ctrl+Z suspends).
 */
void wait_for_job(job_t *j)
{
    int status;
    pid_t pid;

    while (j->nspawned > (j->nexited + j->nstopped)) {
        pid = waitpid(-j->pgid, &status, WUNTRACED);
        if (pid < 0) {
            if (errno == EINTR) continue;
            if (errno == ECHILD) { j->nexited = j->nspawned; break; }
            perror("waitpid");
            break;
        }
        job_update_status(j, pid, status);
        if (WIFSTOPPED(status)) {
            printf("\n[%d]+ Stopped  %s\n", j->jid, j->command);
            j->notified = 1;
        }
    }
    if (j->nexited >= j->nspawned) {
        j->state = JOB_DONE;
    } else if (j->nstopped > 0) {
        j->state = JOB_STOPPED;
    }
    jobs_dirty = 1;
}

/* ------------------------------------------------------------------ */
static void describe(const pipeline_t *pl, char *out, size_t n)
{
    size_t used = 0;
    out[0] = '\0';
    for (int i = 0; i < pl->ncmds; i++) {
        const command_t *c = &pl->cmds[i];
        for (int k = 0; k < c->argc; k++) {
            int w = snprintf(out + used, n - used, "%s%s",
                             (k || i) ? " " : "", c->argv[k]);
            if (w < 0 || (size_t)w >= n - used) { out[n - 1] = '\0'; return; }
            used += w;
        }
        if (i < pl->ncmds - 1) {
            int w = snprintf(out + used, n - used, " |");
            if (w < 0 || (size_t)w >= n - used) return;
            used += w;
        }
    }
    if (pl->background && used + 2 < n) {
        out[used++] = ' ';
        out[used++] = '&';
        out[used]   = '\0';
    }
}

/* ------------------------------------------------------------------ */
/* Redirect the SHELL's own descriptors around a builtin, then
   restore them (so `echo hi > file` works for builtins too).          */
int shell_apply_redirections(command_t *c, int saved[3])
{
    saved[0] = dup(STDIN_FILENO);
    saved[1] = dup(STDOUT_FILENO);
    saved[2] = dup(STDERR_FILENO);
    if (saved[0] < 0 || saved[1] < 0 || saved[2] < 0) return -1;
    return apply_redirections(c);
}

void shell_restore_fds(int saved[3])
{
    fflush(NULL);
    if (saved[0] >= 0) { dup2(saved[0], STDIN_FILENO);  close(saved[0]); }
    if (saved[1] >= 0) { dup2(saved[1], STDOUT_FILENO); close(saved[1]); }
    if (saved[2] >= 0) { dup2(saved[2], STDERR_FILENO); close(saved[2]); }
}

/* ------------------------------------------------------------------ */
int execute_pipeline(pipeline_t *p)
{
    if (p->ncmds == 0) return 0;

    pid_t pgid = -1;
    pid_t pids[MAX_CMDS];
    int   nspawned = 0;
    int   prev_fd = -1;
    int   is_fg = !p->background;
    char  cmdstr[MAX_CMDSTR];
    sigset_t oldmask;

    describe(p, cmdstr, sizeof cmdstr);

    /* Block SIGCHLD for the whole spawn+wait section so the handler
       cannot race with the main loop's bookkeeping (CO-6). */
    block_sigchld(&oldmask);

    for (int i = 0; i < p->ncmds; i++) {
        int fds[2] = { -1, -1 };
        if (i < p->ncmds - 1 && pipe(fds) < 0) {
            perror("forgeshell: pipe");
            break;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("forgeshell: fork");
            if (fds[0] >= 0) { close(fds[0]); close(fds[1]); }
            break;
        }

        if (pid == 0) {
            /* ---------------- child (CO-2) ------------------------ */
            if (pgid == -1) pgid = getpid();
            setpgid(getpid(), pgid);          /* own process group    */

            /* Block job-control signals while touching the terminal:
               the child is still a BACKGROUND process group here, and
               tcsetattr/tcsetpgrp would otherwise raise SIGTTOU and
               stop the child before it even execs (classic race).    */
            sigset_t jc;
            sigemptyset(&jc);
            sigaddset(&jc, SIGTTOU);
            sigaddset(&jc, SIGTTIN);
            sigprocmask(SIG_BLOCK, &jc, NULL);

            restore_cooked_terminal();        /* cooked termios       */
            if (is_fg)
                job_control_give_terminal(pgid); /* controlling tty    */

            child_reset_signals();            /* DFL dispositions + unblock */

            if (prev_fd != -1) {              /* stdin from prev pipe */
                dup2(prev_fd, STDIN_FILENO);
                close(prev_fd);
            }
            if (i < p->ncmds - 1) {           /* stdout to next pipe  */
                dup2(fds[1], STDOUT_FILENO);
                close(fds[0]);
                close(fds[1]);
            }
            if (apply_redirections(&p->cmds[i]) < 0) _exit(1);

            if (!is_fg) {                     /* bg jobs ignore tty signals */
                sigset_t set;
                sigemptyset(&set);
                sigaddset(&set, SIGINT);
                sigaddset(&set, SIGTSTP);
                sigprocmask(SIG_BLOCK, &set, NULL);
            }

            execvp(p->cmds[i].argv[0], p->cmds[i].argv);
            fprintf(stderr, "forgeshell: %s: %s\n",
                    p->cmds[i].argv[0], strerror(errno));
            _exit(127);
        }

        /* ---------------- parent --------------------------------- */
        if (pgid == -1) pgid = pid;
        setpgid(pid, pgid);                  /* race-safe both sides  */
        pids[nspawned++] = pid;

        if (prev_fd != -1) close(prev_fd);
        if (i < p->ncmds - 1) { close(fds[1]); prev_fd = fds[0]; }
    }

    if (nspawned == 0) {
        unblock_sigchld(&oldmask);
        return 0;
    }

    job_t *j = job_add(pgid, pids, nspawned, cmdstr, is_fg);
    if (!j) {
        /* could not track it; just wait so it does not become a zombie */
        for (int i = 0; i < nspawned; i++) waitpid(pids[i], NULL, 0);
        unblock_sigchld(&oldmask);
        return 0;
    }

    if (!is_fg)
        printf("[%d] %d\n", j->jid, (int)pgid);

    /* Publish the job table BEFORE waiting on a foreground job, so the
       UI (and the signal pad) can see it while it runs.               */
    jobs_write_status_file();

    if (is_fg) {
        job_control_give_terminal(pgid);
        wait_for_job(j);
        job_control_take_terminal();

        if (j->state == JOB_DONE) {
            job_remove(j);
        }
    }

    unblock_sigchld(&oldmask);
    jobs_write_status_file();
    return 0;
}
