#include <stdio.h>

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