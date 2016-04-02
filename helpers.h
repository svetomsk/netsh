#ifndef _HELPERS_H_
#define _HELPERS_H_
#include <stdio.h>

ssize_t read_(int fd, void *buf, size_t count);
ssize_t write_(int fd, void *buf, size_t count);

#endif