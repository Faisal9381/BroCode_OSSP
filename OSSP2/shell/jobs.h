/*
 * jobs.h — job table and job-control bookkeeping (CO-3 / CO-6)
 */
#ifndef JOBS_H
#define JOBS_H

#include "shell.h"

typedef enum { JOB_RUNNING = 0, JOB_STOPPED = 1, JOB_DONE = 2 } job_state_t;

typedef struct job {
    int          jid;                 /* 1-based job id              */
    pid_t        pgid;                /* process group id            */
    job_state_t  state;               /* running / stopped / done    */
    int          foreground;          /* 1 if launched in foreground */
    int          notified;            /* notification already printed*/
    int          nspawned;            /* children forked             */
    int          nexited;             /* children that exited        */
    int          nstopped;            /* children that stopped       */
    pid_t        pids[MAX_CMDS];      /* child pids                  */
    int          pstate[MAX_CMDS];    /* 0 alive, 1 exited, 2 stopped*/
    char         command[MAX_CMDSTR]; /* textual command line        */
} job_t;

extern job_t job_table[MAX_JOBS];
extern int   job_count;
extern volatile sig_atomic_t jobs_dirty;  /* job table changed (JSON file) */

void  jobs_init(void);
void  jobs_install_handler(void);
void  fs_debug(const char *fmt, ...);
void  block_sigchld(sigset_t *old);
void  unblock_sigchld(const sigset_t *old);

job_t *job_add(pid_t pgid, pid_t *pids, int n, const char *cmd, int fg);
job_t *job_find_pgid(pid_t pgid);
job_t *job_find_pid(pid_t pid);
job_t *job_find_jid(int jid);
job_t *job_find_spec(const char *spec);      /* "%2", "2", or "sleep"     */
job_t *job_last_stopped(void);
void  job_remove(job_t *j);
void  job_update_status(job_t *j, pid_t pid, int status);

void  jobs_print(int long_format);
int   jobs_print_notifications(void);
void  jobs_write_status_file(void);          /* JSON snapshot for the UI  */
int   jobs_active_count(void);

#endif /* JOBS_H */
