/*
 * builtins.h — shell built-in commands
 */
#ifndef BUILTINS_H
#define BUILTINS_H

#include "shell.h"

typedef int (*builtin_fn)(char **argv);

typedef struct builtin {
    const char *name;
    builtin_fn  fn;
    const char *help;
} builtin_t;

const builtin_t *builtin_lookup(const char *name);
int  run_builtin(const builtin_t *b, char **argv);  /* 0 ok, 1 => exit */

#endif /* BUILTINS_H */
