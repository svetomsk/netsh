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


#endif
