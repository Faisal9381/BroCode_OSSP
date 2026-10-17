/*
 * parser.h — command line parsing (quotes, env expansion, pipes,
 * redirection, background marker)
 */
#ifndef PARSER_H
#define PARSER_H

#include "shell.h"

int  parse_line(const char *line, pipeline_t *pl);   /* 0 ok, -1 error */
void pipeline_free(pipeline_t *pl);

#endif /* PARSER_H */
