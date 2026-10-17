/*
 * shell.h — ForgeShell common definitions
 * Mini Linux Shell with Background Job Management (OSSP project)
 */
#ifndef FORGESHELL_H
#define FORGESHELL_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <limits.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <fcntl.h>

#define SHELL_NAME      "forgeshell"
#define SHELL_VERSION   "1.0"
#define MAX_ARGS        128
#define MAX_CMDS        64      /* max commands in one pipeline  */
#define MAX_JOBS        128
#define MAX_LINE        4096
#define MAX_CMDSTR      512

/* ---- parsed command structures (parser.c) ------------------------- */
typedef struct command {
    char *argv[MAX_ARGS];
    int   argc;
    char *infile;        /* <  file                       */
    char *outfile;       /* > or >> file                  */
    int   out_append;    /* 1 => >>, 0 => >               */
    char *errfile;       /* 2> or 2>> file                */
    int   err_append;
    int   err_to_out;    /* 2>&1                          */
} command_t;

typedef struct pipeline {
    command_t cmds[MAX_CMDS];
    int       ncmds;
    int       background;   /* trailing '&'                 */
} pipeline_t;

/* ---- globals owned by main.c -------------------------------------- */
extern pid_t  shell_pid;          /* pid of the shell itself     */
extern pid_t  shell_pgrp;         /* process group of the shell  */
extern pid_t  shell_terminal;     /* controlling terminal pid    */
extern int    shell_interactive;  /* 1 if running on a tty       */
extern struct termios shell_term_modes;  /* saved cooked modes    */

/* ---- small helpers ------------------------------------------------ */
char *xstrdup(const char *s);
void *xmalloc(size_t n);

#endif /* FORGESHELL_H */
