#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>

#include "helpers.h"

#define BUF_SIZE 1024

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
	printf("stdout before = %i\n", STDIN_FILENO);

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
	check(dup2(stdin_def, STDIN_FILENO), "dup2 failed");
	check(close(stdin_def), "close failed");
	printf("stdout after = %i\n", STDIN_FILENO);

	return 0;
}

execargs_t * make_args_struct(char ** args) {
	execargs_t * cur = (execargs_t *)malloc(sizeof(execargs_t *));
	cur->command = args[0];
	cur->args = args;
	return cur;
}

void free_args_struct(execargs_t * cur) {
	free(cur->args);
	free(cur);
}

char * substring(const char * input, int offset, int len) {
	char * dest = malloc(sizeof(char *));
	strncpy(dest, input + offset, len);
	dest[len] = 0;
	return dest;
}

int is_whitespace(char c) {
	return (c == ' ' || c == '\t' || c == '\r' || c == '\n');
}

char ** to_array(char * s) {
	size_t pos = 0;
	size_t len = strlen(s);
	size_t count = 0;
	// count words
	while(pos < len) {
		//skip spaces
		while(pos < len && is_whitespace(s[pos])){
			pos++;
		}
		size_t start = pos;
		int word = 0;
		while(pos < len && !is_whitespace(s[pos])){
			word = 1;
			pos++;
		}
		if(word) {
			count++;
		}
	}

	char ** cur = malloc((count + 1) * sizeof(char *));
	pos = 0;
	for(int i = 0; i < count; i++) {
		while(pos < len && is_whitespace(s[pos])) {
			pos++;
		}
		size_t start = pos;
		int word = 0;
		while(pos < len && !is_whitespace(s[pos])) {
			pos++;
			word = 1;
		}
		if(word) {
			cur[i] = substring(s, start, pos - start);
			printf("!%s!\n", cur[i]);
		}
	}
	cur[count] = 0;
	return cur;
}

static int commands_count;

int get_commands_count() {
	return commands_count;
}

execargs_t ** read_and_split_in_commands(char * input) {
	char * buf = input;
	// printf("read from %i\n", in);
	// ssize_t cur_read = read_(in, buf, BUF_SIZE);
	// check(cur_read, "error in read_");
	// buf[cur_read] = 0;
	//count commands;
	int count = 1;
	size_t len = strlen(buf);
	for(size_t i = 0; i < len; i++) {
		if(buf[i] == '|') {
			count++;
		}
	}
	commands_count = count;

	execargs_t ** commands = malloc(count * sizeof(execargs_t *));
	ssize_t pos = 0;
	for(int i = 0; i < count; i++) {
		ssize_t start = pos;
		while(pos < len && buf[pos] != '|') {
			pos++;
		}
		pos++;
		char * command = substring(buf, start, pos - start - 1);
		char ** cur = to_array(command);
		commands[i] = make_args_struct(cur);
	}

	return commands;
}
