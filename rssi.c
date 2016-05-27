#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "ToB.h"

char buf[BUFSIZ];

int main(int argc, char *argv[])
{
	fd_set readfds;
	int fd, ret;
	ssize_t size;
	struct sockaddr_in remote;
	struct timeval timeout;
	char rssi;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s serverAddress\n", argv[0]);
		return errno;
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

	srandom(time(NULL));

	while (1) {
		/* init */
		FD_ZERO(&readfds);
		FD_SET(fd, &readfds);

		/* send random generated rssi per 10 seconds */
		memset(&timeout, 0, sizeof(timeout));
		timeout.tv_sec = 10;

		ret = select(fd + 1, &readfds, NULL, NULL, &timeout);

		if (ret < 0) {
			perror("select");
			break;
		} else if (ret == 0) {
			/* timeout, send random generated rssi */
			/* range: -1~-10, not in range: 0 */
			rssi = -(random() % 11);

			if (rssi == 0)
				printf("<-- NOT in Range\n");
			else
				printf("<-- rssi = %hhd\n", rssi);

			size = snprintf(buf, sizeof(buf), "%hhd", rssi);

			size = send(fd, buf, size, 0);
			if (size < 0)
				perror("send");
			if (size <= 0)
				break;

			continue;
		}

		if (FD_ISSET(fd, &readfds)) {
			size = recv(fd, buf, sizeof(buf), 0);
			if (size < 0)
				perror("recv");
			if (size <= 0)
				break;
			size = write(STDOUT_FILENO, buf, size);
			if (size < 0)
				perror("write");
			if (size <= 0)
				break;
		}
	}

	return errno;
}
