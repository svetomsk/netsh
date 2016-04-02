#ifndef _HELPERS_H_
#define _HELPERS_H_
#include <stdio.h>
#include <sys/types.h>

typedef struct execargs_t {
	char * command;
	char ** args;
} execargs_t;

ssize_t read_(int fd, void *buf, size_t count);
ssize_t write_(int fd, void *buf, size_t count);

int exec(execargs_t * );
int runpiped(execargs_t ** commands, size_t count);

execargs_t * make_args_struct(char ** args);
void free_args_struct(execargs_t *);

execargs_t ** read_and_split_in_commands(char * in);
int get_commands_count();

#endif
