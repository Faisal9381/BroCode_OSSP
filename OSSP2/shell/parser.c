/* 
 * parser.c — command line parsing
 *
 * Features: single/double quotes, $VAR / ${VAR} expansion, ~ expansion,
 * pipelines (|), input/output/append redirection (<, >, >>),
 * stderr redirection (2>, 2>>), stderr-to-stdout (2>&1) and the
 * background marker (&).
 */
#define _GNU_SOURCE
#include <ctype.h>
#include "shell.h"
#include "parser.h"

/* ------------------------------------------------------------------ */
static void syntax_error(const char *msg)
{
    fprintf(stderr, "forgeshell: syntax error: %s\n", msg);
}

/* ------------------------------------------------------------------ */
/*
 * Scan one (possibly quoted) word starting at *pp, appending the
 * expanded result to *out (realloc'd). Returns 0 on success, -1 on
 * unterminated quote.
 */
static int scan_word(const char **pp, char **out, size_t *len, size_t *cap)
{
    const char *p = *pp;
    int saw_something = 0;

    while (*p && *p != ' ' && *p != '\t' && *p != '|' && *p != '&' &&
           *p != '<' && *p != '>') {

        if (*p == '\'') {                       /* single quotes: literal */
            p++;
            while (*p && *p != '\'') {
                if (*len + 1 >= *cap) {
                    *cap = *cap ? *cap * 2 : 128;
                    *out = realloc(*out, *cap);
                }
                (*out)[(*len)++] = *p++;
            }
            if (*p != '\'') return -1;          /* unterminated */
            p++;
            saw_something = 1;
        } else if (*p == '"') {                 /* double quotes: $ works */
            p++;
            while (*p && *p != '"') {
                if (*p == '$') {
                    p++;
                    const char *name = p;
                    char nbuf[256];
                    size_t ni = 0;
                    if (*p == '{') {
                        p++;
                        name = p;
                        while (*p && *p != '}' && ni < sizeof nbuf - 1)
                            nbuf[ni++] = *p++;
                        if (*p == '}') p++;
                    } else {
                        while ((isalpha((unsigned char)*p) ||
                                isdigit((unsigned char)*p) ||
                                *p == '_') && ni < sizeof nbuf - 1)
                            nbuf[ni++] = *p++;
                    }
                    nbuf[ni] = '\0';
                    const char *val = (ni ? getenv(nbuf) : NULL);
                    if (val) {
                        size_t vl = strlen(val);
                        while (*len + vl >= *cap) {
                            *cap = *cap ? *cap * 2 : 128;
                            *out = realloc(*out, *cap);
                        }
                        memcpy(*out + *len, val, vl);
                        *len += vl;
                    }
                    (void)name;
                } else {
                    if (*len + 1 >= *cap) {
                        *cap = *cap ? *cap * 2 : 128;
                        *out = realloc(*out, *cap);
                    }
                    (*out)[(*len)++] = *p++;
                }
            }
            if (*p != '"') return -1;
            p++;
            saw_something = 1;
        } else if (*p == '$' && (p[1] == '{' ||
                   isalpha((unsigned char)p[1]) || p[1] == '_')) {
            p++;
            char nbuf[256];
            size_t ni = 0;
            if (*p == '{') {
                p++;
                while (*p && *p != '}' && ni < sizeof nbuf - 1)
                    nbuf[ni++] = *p++;
                if (*p == '}') p++;
            } else {
                while ((isalpha((unsigned char)*p) ||
                        isdigit((unsigned char)*p) || *p == '_') &&
                       ni < sizeof nbuf - 1)
                    nbuf[ni++] = *p++;
            }
            nbuf[ni] = '\0';
            const char *val = ni ? getenv(nbuf) : NULL;
            if (val) {
                size_t vl = strlen(val);
                while (*len + vl >= *cap) {
                    *cap = *cap ? *cap * 2 : 128;
                    *out = realloc(*out, *cap);
                }
                memcpy(*out + *len, val, vl);
                *len += vl;
            }
            saw_something = 1;
        } else if (*p == '~' && (p[1] == '/' || p[1] == '\0' ||
                   p[1] == ' ' || !saw_something)) {
            const char *home = getenv("HOME");
            if (!home) home = "";
            size_t hl = strlen(home);
            while (*len + hl >= *cap) {
                *cap = *cap ? *cap * 2 : 128;
                *out = realloc(*out, *cap);
            }
            memcpy(*out + *len, home, hl);
            *len += hl;
            p++;
            saw_something = 1;
        } else {
            if (*len + 1 >= *cap) {
                *cap = *cap ? *cap * 2 : 128;
                *out = realloc(*out, *cap);
            }
            (*out)[(*len)++] = *p++;
            saw_something = 1;
        }
    }
    *pp = p;
    return 0;
}

/* ------------------------------------------------------------------ */
static void free_command(command_t *c)
{
    for (int i = 0; i < c->argc; i++) free(c->argv[i]);
    c->argc = 0;
    free(c->infile);  c->infile  = NULL;
    free(c->outfile); c->outfile = NULL;
    free(c->errfile); c->errfile = NULL;
}

void pipeline_free(pipeline_t *pl)
{
    for (int i = 0; i < pl->ncmds; i++)
        free_command(&pl->cmds[i]);
    pl->ncmds = 0;
    pl->background = 0;
}

/* ------------------------------------------------------------------ */
int parse_line(const char *line, pipeline_t *pl)
{
    memset(pl, 0, sizeof *pl);
    const char *p = line;

    /* trivially empty */
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '\0' || *p == '#') return 0;   /* nothing to do */

    command_t *cur = &pl->cmds[0];
    pl->ncmds = 1;
    int expect_word_for = 0;  /* 1=<  2=>  3=2> */

    for (;;) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;

        /* ---- operators ------------------------------------------- */
        if (*p == '|') {
            if (cur->argc == 0) { syntax_error("near unexpected token `|'"); return -1; }
            if (pl->ncmds >= MAX_CMDS) { syntax_error("pipeline too long"); return -1; }
            p++;
            cur = &pl->cmds[pl->ncmds++];
            continue;
        }
        if (*p == '&') {
            if (cur->argc == 0 && !expect_word_for) {
                syntax_error("near unexpected token `&'");
                return -1;
            }
            if (p[1] == '&') { syntax_error("`&&' is not supported"); return -1; }
            pl->background = 1;
            p++;
            continue;
        }
        if (*p == '<') {
            if (cur->infile) { syntax_error("multiple input redirects"); return -1; }
            p++;
            expect_word_for = 1;
            continue;
        }
        if (*p == '>') {
            if (p[1] == '>') {
                if (cur->outfile) { syntax_error("multiple output redirects"); return -1; }
                p += 2;
                cur->out_append = 1;
            } else {
                if (cur->outfile) { syntax_error("multiple output redirects"); return -1; }
                p++;
                cur->out_append = 0;
            }
            expect_word_for = 2;
            continue;
        }
        if (*p == '2' && p[1] == '>') {
            if (p[2] == '&') {                 /* 2>&1 */
                if (cur->errfile) { syntax_error("conflicting stderr redirect"); return -1; }
                p += 3;
                if (*p == '1') p++;             /* consume the trailing 1 */
                cur->err_to_out = 1;
                continue;
            }
            if (cur->errfile) { syntax_error("multiple stderr redirects"); return -1; }
            if (p[2] == '>') { p += 3; cur->err_append = 1; }
            else             { p += 2; cur->err_append = 0; }
            expect_word_for = 3;
            continue;
        }

        /* ---- words ------------------------------------------------ */
        char *word = NULL;
        size_t len = 0, cap = 0;
        if (scan_word(&p, &word, &len, &cap) < 0) {
            free(word);
            syntax_error("unterminated quote");
            return -1;
        }
        if (len == 0 && !word) { syntax_error("empty word"); return -1; }
        if (!word) word = xstrdup("");
        word[len] = '\0';

        if (expect_word_for == 1)      { cur->infile  = word; expect_word_for = 0; }
        else if (expect_word_for == 2) { cur->outfile = word; expect_word_for = 0; }
        else if (expect_word_for == 3) { cur->errfile = word; expect_word_for = 0; }
        else {
            if (cur->argc >= MAX_ARGS - 1) {
                free(word);
                syntax_error("too many arguments");
                return -1;
            }
            cur->argv[cur->argc++] = word;
            cur->argv[cur->argc] = NULL;
        }
    }

    if (expect_word_for) { syntax_error("missing filename after redirect"); return -1; }
    if (cur->argc == 0)  { syntax_error("empty command"); return -1; }
    if (pl->background && pl->ncmds == 1 && cur->argc == 0) {
        syntax_error("empty command");
        return -1;
    }
    return 0;
}
