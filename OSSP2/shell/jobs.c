/*
 * jobs.c — job table, SIGCHLD reaping, notifications, JSON status file
 *
 * Concurrency model (CO-6):
 *   - While the shell is at the prompt, SIGCHLD is unblocked and the
 *     handler reaps children with waitpid(WNOHANG|WUNTRACED) and updates
 *     the job table (async-signal-safe: only waitpid + plain stores).
 *   - Whenever the main loop mutates the table or waits for a foreground
 *     job it blocks SIGCHLD first, so handler and main loop never race.
 */
#define _GNU_SOURCE
#include <stdarg.h>
#include "shell.h"
#include "jobs.h"
#include "execute.h"

job_t job_table[MAX_JOBS];
int   job_count = 0;
volatile sig_atomic_t jobs_dirty = 0;

/* Optional debug log (FORGESHELL_DEBUG=/path/file).  Uses only
   open/vsnprintf/write so it is async-signal-safe enough to call
   from the SIGCHLD handler while diagnosing races.                  */
static int  dbgfd = -1;
static int  dbg_ready = 0;
void fs_debug(const char *fmt, ...)
{
    if (!dbg_ready) {
        const char *p = getenv("FORGESHELL_DEBUG");
        if (p && *p)
            dbgfd = open(p, O_WRONLY | O_CREAT | O_APPEND, 0644);
        dbg_ready = 1;
    }
    if (dbgfd < 0) return;
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > 0) {
        if ((size_t)n > sizeof buf) n = (int)sizeof buf;
        ssize_t w = write(dbgfd, buf, (size_t)n);
        (void)w;
    }
}

/* ------------------------------------------------------------------ */
void jobs_init(void)
{
    memset(job_table, 0, sizeof job_table);
    job_count = 0;
}

void block_sigchld(sigset_t *old)
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    sigprocmask(SIG_BLOCK, &set, old);
}

void unblock_sigchld(const sigset_t *old)
{
    sigprocmask(SIG_SETMASK, old, NULL);
}

/* ------------------------------------------------------------------ */
static int jid_in_use(int jid)
{
    for (int i = 0; i < MAX_JOBS; i++)
        if (job_table[i].nspawned && job_table[i].jid == jid)
            return 1;
    return 0;
}

static int alloc_jid(void)
{
    /* lowest free job id, like bash */
    for (int j = 1; ; j++)
        if (!jid_in_use(j))
            return j;
}

job_t *job_add(pid_t pgid, pid_t *pids, int n, const char *cmd, int fg)
{
    job_t *j = NULL;
    for (int i = 0; i < MAX_JOBS; i++)
        if (job_table[i].nspawned == 0) { j = &job_table[i]; break; }
    if (!j) {
        fprintf(stderr, "forgeshell: too many jobs\n");
        return NULL;
    }
    memset(j, 0, sizeof *j);
    j->jid        = alloc_jid();
    j->pgid       = pgid;
    j->state      = JOB_RUNNING;
    j->foreground = fg;
    j->nspawned   = n;
    j->nexited    = 0;
    j->nstopped   = 0;
    for (int i = 0; i < n; i++) {
        j->pids[i]  = pids[i];
        j->pstate[i] = 0;
    }
    snprintf(j->command, sizeof j->command, "%s", cmd);
    job_count++;
    jobs_dirty = 1;
    return j;
}

job_t *job_find_pgid(pid_t pgid)
{
    for (int i = 0; i < MAX_JOBS; i++)
        if (job_table[i].nspawned && job_table[i].pgid == pgid)
            return &job_table[i];
    return NULL;
}

job_t *job_find_pid(pid_t pid)
{
    for (int i = 0; i < MAX_JOBS; i++) {
        job_t *j = &job_table[i];
        if (!j->nspawned) continue;
        for (int k = 0; k < j->nspawned; k++)
            if (j->pids[k] == pid) return j;
    }
    return NULL;
}

job_t *job_find_jid(int jid)
{
    for (int i = 0; i < MAX_JOBS; i++)
        if (job_table[i].nspawned && job_table[i].jid == jid)
            return &job_table[i];
    return NULL;
}

/* spec: "%2", "2" or a command-name prefix such as "sleep"          */
job_t *job_find_spec(const char *spec)
{
    if (!spec || !*spec) return NULL;
    if (spec[0] == '%') spec++;
    if (*spec >= '0' && *spec <= '9')
        return job_find_jid(atoi(spec));
    job_t *found = NULL;
    for (int i = 0; i < MAX_JOBS; i++) {
        job_t *j = &job_table[i];
        if (!j->nspawned) continue;
        if (strncmp(j->command, spec, strlen(spec)) == 0)
            found = j;               /* last match wins */
    }
    return found;
}

job_t *job_last_stopped(void)
{
    job_t *found = NULL;
    for (int i = 0; i < MAX_JOBS; i++)
        if (job_table[i].nspawned && job_table[i].state == JOB_STOPPED)
            found = &job_table[i];
    return found;
}

void job_remove(job_t *j)
{
    if (!j || !j->nspawned) return;
    j->nspawned = 0;
    j->pgid     = 0;
    job_count--;
    jobs_dirty  = 1;
}

int jobs_active_count(void)
{
    int n = 0;
    for (int i = 0; i < MAX_JOBS; i++)
        if (job_table[i].nspawned) n++;
    return n;
}

/* ------------------------------------------------------------------ */
/* Update one job after waitpid() reported on one of its children.     */
/* Returns nothing; may be called from the SIGCHLD handler.            */
void job_update_status(job_t *j, pid_t pid, int status)
{
    if (!j) return;
    for (int k = 0; k < j->nspawned; k++) {
        if (j->pids[k] != pid) continue;

        if (WIFSTOPPED(status)) {
            if (j->pstate[k] != 2) { j->pstate[k] = 2; j->nstopped++; }
        } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
            if (j->pstate[k] == 0) { j->pstate[k] = 1; j->nexited++; }
        }

        if (j->nexited >= j->nspawned) {
            j->state     = JOB_DONE;
            j->notified  = 0;
        } else if (j->nstopped > 0 && j->nexited + j->nstopped >= j->nspawned) {
            j->state     = JOB_STOPPED;
            j->notified  = 0;
        } else {
            j->state     = JOB_RUNNING;
        }
        jobs_dirty = 1;
        return;
    }
}

/* ------------------------------------------------------------------ */
/* SIGCHLD handler: reap every child that has changed state.           */
void sigchld_handler(int sig)
{
    (void)sig;
    int saved = errno;
    pid_t pid;
    int status;

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        job_t *j = job_find_pid(pid);
        fs_debug("[handler] reaped pid=%d sig=%d jid=%d\n", (int)pid,
                 WIFSIGNALED(status) ? WTERMSIG(status) : -1,
                 j ? j->jid : -1);
        job_update_status(j, pid, status);
    }
    fs_debug("[handler] pass last=%d\n", (int)pid);
    errno = saved;
}

void jobs_install_handler(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    /* No SA_RESTART: SIGCHLD must interrupt read() at the prompt so
       the shell can print job notifications immediately (like bash)
       and redraw the line being edited.                            */
    sa.sa_flags = 0;
    sigaction(SIGCHLD, &sa, NULL);
}

/* ------------------------------------------------------------------ */
static const char *state_str(job_state_t s)
{
    switch (s) {
    case JOB_RUNNING: return "Running";
    case JOB_STOPPED: return "Stopped";
    case JOB_DONE:    return "Done";
    }
    return "?";
}

void jobs_print(int long_format)
{
    for (int i = 0; i < MAX_JOBS; i++) {
        job_t *j = &job_table[i];
        if (!j->nspawned) continue;
        char mark = (j->state == JOB_STOPPED) ? '-' :
                    (j->state == JOB_DONE)    ? '+' : ' ';
        if (long_format)
            printf("[%d]%c %-8s %6d  %s\n",
                   j->jid, mark, state_str(j->state), (int)j->pgid, j->command);
        else
            printf("[%d]%c %-8s %s\n",
                   j->jid, mark, state_str(j->state), j->command);
    }
}

/* Called right before printing a new prompt (main loop context).
   Returns the number of notifications printed, so the caller can
   re-check after unblocking SIGCHLD (close the reap race).          */
int jobs_print_notifications(void)
{
    int printed = 0;
    for (int i = 0; i < MAX_JOBS; i++) {
        job_t *j = &job_table[i];
        if (!j->nspawned || j->notified) continue;

        if (j->state == JOB_DONE) {
            fs_debug("[notify] jid=%d Done\n", j->jid);
            printf("[%d]+ Done     %s\n", j->jid, j->command);
            job_remove(j);
            printed++;
        } else if (j->state == JOB_STOPPED) {
            printf("[%d]+ Stopped  %s\n", j->jid, j->command);
            j->notified = 1;
            printed++;
        }
    }
    return printed;
}

/* ------------------------------------------------------------------ */
/* JSON snapshot of the REAL job table, written for the web UI.        */
/* Written atomically (tmp file + rename); enabled with               */
/* FORGESHELL_JOBS_FILE=<path> in the environment.                    */
void jobs_write_status_file(void)
{
    const char *path = getenv("FORGESHELL_JOBS_FILE");
    if (!path || !*path) return;

    char tmppath[PATH_MAX];
    snprintf(tmppath, sizeof tmppath, "%s.tmp", path);

    FILE *f = fopen(tmppath, "w");
    if (!f) return;

    fprintf(f, "[");
    int first = 1;
    for (int i = 0; i < MAX_JOBS; i++) {
        job_t *j = &job_table[i];
        if (!j->nspawned) continue;
        if (!first) fprintf(f, ",");
        first = 0;
        /* escape the command text minimally for JSON */
        fprintf(f, "\n  {\"jid\":%d,\"pid\":%d,\"state\":\"%s\","
                   "\"foreground\":%s,\"command\":\"",
                   j->jid, (int)j->pgid, state_str(j->state),
                   j->foreground ? "true" : "false");
        for (const char *c = j->command; *c; c++) {
            if (*c == '"' || *c == '\\') fputc('\\', f);
            if (*c == '\n') { fputs("\\n", f); continue; }
            fputc(*c, f);
        }
        fprintf(f, "\"}");
    }
    fprintf(f, "\n]\n");
    fclose(f);

    if (rename(tmppath, path) != 0)
        remove(tmppath);
}
