#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <netdb.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/epoll.h>

#include "helpers.h"

void test(int result, char * msg) {
	if(result == -1) {
		perror(msg);
		exit(EXIT_FAILURE);
	}
}

int create_and_bind_socket(int port) {
	char port_str[10];
	sprintf(port_str, "%d", port);
	int status;
	struct addrinfo hints;
	struct addrinfo * resinfo;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC; // return v4 and v6 choices
	hints.ai_socktype = SOCK_STREAM; // TCP socket

	status = getaddrinfo("localhost", port_str, &hints, &resinfo);
	test(status, "getaddrinfo failed");

	int sockfd;
	struct addrinfo * i;
	for(i = resinfo; i != NULL; i = i->ai_next) {
		// set soket CLOEXEC to close after process finish
		sockfd = socket(i->ai_family, i->ai_socktype | SOCK_CLOEXEC, i->ai_protocol);
		if(sockfd == -1) continue;
		int yes = 1;
		status = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
		if(status == -1) continue;
		status = bind(sockfd, i->ai_addr, i->ai_addrlen);
		if(status == 0) break;
		close(sockfd);
	}
	if(i == NULL) return EXIT_FAILURE;
	freeaddrinfo(resinfo);
	return sockfd;
}

int make_non_blocking(int sockfd) {
	int flags;
	int status;
	flags = fcntl(sockfd, F_GETFL, 0);
	test(flags, "fcntl returned -1");
	flags |= O_NONBLOCK;
	status = fcntl(sockfd, F_SETFL, flags);
	test(status, "fcntl return -1");
	return EXIT_SUCCESS;
}

int create_server_socket(int port) {
	int fd = create_and_bind_socket(port);
	test(fd, "server socket not created");
	test(listen(fd, SOMAXCONN), "listening on server socket failed");
	test(make_non_blocking(fd), "make server socket non blocking failed");
	return fd;
}


#define MAXEVENTS 16

int main(int argc, char ** argv) {
	if(argc != 2) {
		perror("Usage: ./netsh <port>");
		return -1;
	}

	int port = atoi(argv[1]);

	int child = fork();
	test(child, "fork error");
	if(child == 0) {
		int id = setsid();
		test(id, "setsid error");
		child = fork();
		test(child, "fork error");
		if(child == 0) {
			int pid_file = open("/tmp/netsh.pid", O_CLOEXEC | O_CREAT | O_RDWR);
			test(pid_file, "open failed");
			printf("FILE DESCR %i\n", pid_file);
			char buf[4];
			sprintf(buf, "%d", getpid());
			int w = write(pid_file, buf, 4);
			close(pid_file);

			int serv_sock, sock;
			int epoll_fd;
			struct epoll_event event;
			struct epoll_event *events;

			serv_sock = create_and_bind_socket(port);
			sock = make_non_blocking(serv_sock);
			sock = listen(serv_sock, SOMAXCONN);

			epoll_fd = epoll_create1(0);
			test(epoll_fd, "epoll_create1");

			event.data.fd = serv_sock;
			event.events = EPOLLIN | EPOLLET;
			sock = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, serv_sock, &event);
			test(sock, "epoll_ctl");

			events = calloc(MAXEVENTS, sizeof(event)); // buffer for data to be returned

			//main loop
			while(1) {
				int n, i;
				printf("Waiting for events\n");
				n = epoll_wait(epoll_fd, events, MAXEVENTS, -1);
				printf("Got %i events to process\n", n);
				for(i = 0; i < n; i++) {
					if((events[i].events & EPOLLERR) ||
						(events[i].events & EPOLLHUP) ||
						(!(events[i].events & EPOLLIN))) {
						perror("closing socket");
						close(events[i].data.fd);
						continue;
					} else if(serv_sock == events[i].data.fd) {
						//we have incoming connections
						while(1) {
							struct sockaddr in_addr;
							socklen_t in_len;
							int in_fd;
							char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
							in_len = sizeof(in_addr);
							in_fd = accept4(serv_sock, &in_addr, &in_len, SOCK_CLOEXEC);

							if(in_fd == -1) {
								if((errno == EAGAIN) ||
									(errno == EWOULDBLOCK)) {
									break;
									// no more incoming connections
								} else {
									perror("accept error");
									break;
								}
							}
							sock = getnameinfo(&in_addr, in_len, hbuf, sizeof(hbuf),
								sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);
							if(sock == 0) {
								printf("Accepted connnection\n");
							}
							sock = make_non_blocking(in_fd);
							
							event.data.fd = in_fd;
							event.events = EPOLLIN | EPOLLET;
							sock = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, in_fd, &event);
							test(sock, "epoll_ctl");
						}
						continue;
					} else {
						// we have data to be read
						ssize_t count;
						char buf[512];
						int cur_read = read(events[i].data.fd, buf, sizeof(buf));
						test(cur_read, "read error");
						buf[cur_read] = 0;
						if(strlen(buf) == 0) { // ^C ^D and others
							continue;
						}
						execargs_t ** cur = read_and_split_in_commands(buf);
						int out = dup(STDOUT_FILENO);
						test(out, "dup error");
						test(dup2(events[i].data.fd, STDOUT_FILENO), "dup2 error");
						int res = runpiped(cur, get_commands_count());
						test(dup2(out, STDOUT_FILENO), "dup2 error");
						test(close(out), "close error");
					}
				}
			}	
			free(events);
			close(serv_sock);
		}
	}
	return EXIT_SUCCESS;

}