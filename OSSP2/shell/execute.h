/*
 * execute.h — spawning pipelines, waiting, terminal hand-off
 */
#ifndef EXECUTE_H
#define EXECUTE_H

#include "shell.h"

int   execute_pipeline(pipeline_t *p);   /* 0 ok, 1 => shell should exit */
int   shell_apply_redirections(command_t *c, int saved[3]);
void  shell_restore_fds(int saved[3]);
void  wait_for_job(job_t *j);
void  job_control_give_terminal(pid_t pgid);
void  job_control_take_terminal(void);
void  restore_cooked_terminal(void);

#endif /* EXECUTE_H */
