#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <linux/in.h>
#include <pthread.h>
#include "ToB.h"

const char waiting_message[] = "Please wait someone to be online\n";
const char open_message[] = "Conversation opens\n";
char buf[BUFSIZ];

struct pair {
	int fd1, fd2;
};

void *hotspot_handler(void *arg)
{
	struct pair *pair;
	fd_set readfds;
	int nfds, ret;
	ssize_t size;

	/* must have an argument */
	if (arg)
		pair = arg;
	else
		return NULL;

	send(pair->fd1, open_message, sizeof(open_message), 0);
	send(pair->fd2, open_message, sizeof(open_message), 0);

	while (1) {
		/* init */
		FD_ZERO(&readfds);
		FD_SET(pair->fd1, &readfds);
		FD_SET(pair->fd2, &readfds);

		nfds = (pair->fd1 > pair->fd2) ? pair->fd1 : pair->fd2;
		ret = select(++nfds, &readfds, NULL, NULL, NULL);

		if (ret == -1) {
			perror("select");
			continue;
		} else if (ret == 0) {
			perror("No data");
			continue;
		}

		if (FD_ISSET(pair->fd1, &readfds)) {
#ifdef DEBUG
			printf("fd1 has data!!\n");
			memset(buf, 0, sizeof(buf));
#endif
			size = recv(pair->fd1, buf, sizeof(buf), 0);
			if (size <= 0) {
				perror("recv");
				break;
			}
#ifdef DEBUG
			fprintf(stderr, "len = %d, %s\n", (int)size, buf);
#endif
			size = send(pair->fd2, buf, size, 0);
			if (size <= 0) {
				perror("send");
				break;
			}
		}

		if (FD_ISSET(pair->fd2, &readfds)) {
#ifdef DEBUG
			printf("fd2 has data!!\n");
			memset(buf, 0, sizeof(buf));
#endif
			size = recv(pair->fd2, buf, sizeof(buf), 0);
			if (size <= 0) {
				perror("recv");
				break;
			}
#ifdef DEBUG
			fprintf(stderr, "len = %d, %s\n", (int)size, buf);
#endif
			size = send(pair->fd1, buf, size, 0);
			if (size <= 0) {
				perror("send");
				break;
			}
		}
	}

	close(pair->fd1);
	close(pair->fd2);
	free(pair);

	return NULL;
}

int main(int argc, char *argv[])
{
	int fd, fd1, fd2, ret;
	struct sockaddr_in local, remote1, remote2;
	socklen_t len;	
	struct pair *pair;
	pthread_t thread;
	
	/* create TCP socket */
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket");
		return errno;
	}

	ret = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &ret, sizeof(ret)) < 0) {
		perror("setsockopt(SO_REUSEADDR)");
		return errno;
	}

	memset(&local, 0, sizeof(local));
	local.sin_family = AF_INET;
	local.sin_port = htons(SERVER_PORT);

	ret = bind(fd, (struct sockaddr *)&local, sizeof(local));
	if (ret) {
		perror("bind");
		return errno;
	}

	ret = listen(fd, 0);
	if (ret) {
		perror("listen");
		return errno;
	}

	do {
		/* wait two friends to be online, and start conversation */
		len = sizeof(remote1);
		fd1 = accept(fd, (struct sockaddr *)&remote1, &len);
		if (fd1 < 0) {
			perror("accept remote1");
			continue;
		}
		send(fd1, waiting_message, sizeof(waiting_message), 0);

		len = sizeof(remote2);
		fd2 = accept(fd, (struct sockaddr *)&remote2, &len);
		if (fd2 < 0) {
			perror("accept remote2");
			continue;
		}

		pair = malloc(sizeof(struct pair));
		if (pair == NULL) {
			perror("malloc");
			return errno;
		}
		pair->fd1 = fd1;
		pair->fd2 = fd2;

		pthread_create(&thread, NULL, hotspot_handler, pair);
	} while (1);

	perror("accept");

	return errno;
}
