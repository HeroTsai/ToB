#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "ToB.h"

char buf[BUFSIZ];

int main(int argc, char *argv[])
{
	fd_set readfds;
	int fd, nfds, ret;
	ssize_t size;
	struct sockaddr_in remote;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s serverAddress\n", argv[0]);
	}

	/* create TCP socket */
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket");
		return errno;
	}

	memset(&remote, 0, sizeof(remote));
	remote.sin_family = AF_INET;
	inet_pton(AF_INET, argv[1], &remote.sin_addr);
	remote.sin_port = htons(SERVER_PORT);

	ret = connect(fd, (struct sockaddr *)&remote, sizeof(remote));
	if (ret) {
		perror("connect");
		return errno;
	}

	while (1) {
		/* init */
		FD_ZERO(&readfds);
		FD_SET(fd, &readfds);
		FD_SET(STDIN_FILENO, &readfds);

		nfds = (fd > STDIN_FILENO) ? fd : STDIN_FILENO;
		ret = select(++nfds, &readfds, NULL, NULL, NULL);

		if (ret < 0) {
			perror("select");
			break;
		} else if (ret == 0) {
			perror("No data");
			break;
		}

		if (FD_ISSET(fd, &readfds)) {
			size = recv(fd, buf, sizeof(buf), 0);
			if (size <= 0) {
				perror("recv");
				break;
			}
			size = write(STDOUT_FILENO, buf, size);
			if (size <= 0) {
				perror("write");
				break;
			}
		}

		if (FD_ISSET(STDIN_FILENO, &readfds)) {
			size = read(STDIN_FILENO, buf, sizeof(buf));
			if (size <= 0) {
				perror("read");
				break;
			}
			size = send(fd, buf, size, 0);
			if (size <= 0) {
				perror("send");
				break;
			}
		}
	}

	return errno;
}
