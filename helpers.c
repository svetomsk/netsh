#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

#include "helpers.h"

void check(int result, char * msg) {
	if(result == -1) {
		perror(msg);
		exit(EXIT_FAILURE);
	}
}

ssize_t read_(int fd, void *buf, size_t count) {
	ssize_t total = 0;
	ssize_t cur_read = 0;

	while(total < count) {
		cur_read = read(fd, buf + total, count - total);
		if(cur_read < 0) { //error occured
			return cur_read;
		} else if(cur_read == 0) { // EOF
			break;
		} else {
			total += cur_read;
		}
	}
	return total;
}

ssize_t write_(int fd, void *buf, size_t count) {
	ssize_t total = 0;
	ssize_t cur_write = 0;

	while(total < count) {
		cur_write = write(fd, buf + total, count - total);
		if(cur_write < 0) { //error occured
			return cur_write;
		} else {
			total += cur_write;
		}
	}

	return cur_write;
}

int exec(execargs_t * execargs) {
	pid_t child = fork();
	if(child == -1) { //error occured
		return -1;
	}
	if(child == 0) { //child process
		if(execvp(execargs->command, execargs->args) == -1) { //error occured
			return -1;
		}
	}
	return child;
}

static int * children;
static int count;
static int stdin_def;
static int stdout_def;

int runpiped(execargs_t ** commands, size_t n) {
	int pipefd[2];
	//save old values of STDIN and STDOUT
	stdin_def = dup(STDIN_FILENO);
	check(stdin_def, "dup error stdin_def");
	stdout_def = dup(STDOUT_FILENO);
	check(stdout_def, "dup error stdout_def");

	int a[n];
	for(size_t i = 0; i < count; i++) {
		a[i] = -1;
	}
	children = a;
	count = n;

	for(int i = 0; i < n; i++) {
		if(i != n - 1) {
			check(pipe(pipefd), "pipe failed"); //create pipe 1 - write, 0 - read
			check(dup2(pipefd[1], STDOUT_FILENO), "dup2 failed"); // STDOUT -> pipe
			check(close(pipefd[1]), "close failed");
		} else { // last program
			check(dup2(stdout_def, STDOUT_FILENO), "dup2 failed");
			check(close(stdout_def), "close failed");
		}
		children[i] = exec(commands[i]);
		check(children[i], "exec failed");

		if(i != n - 1) {
			check(dup2(pipefd[0], STDIN_FILENO), "dup2 failed");
			check(close(pipefd[0]), "close failed");
		} else {
			check(close(STDIN_FILENO), "close failed");
		}
	}

	for(int i = 0; i < count; i++) {
		wait(NULL);
	}

	//restore context
	check(dup2(stdin_def, STDIN_FILENO), "dup22 failed");

	return 0;
}
