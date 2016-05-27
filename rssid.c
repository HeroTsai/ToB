#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "ToB.h"

const char connected_message[] = "--> Connected to the server\n";
const char connect_message[] = "--> Please CONNECT to the device\n";
const char disconnect_message[] = "--> Please DISCONNECTION from the device\n";
char buf[BUFSIZ];

/* mutex needed */
pthread_mutex_t mutex;

struct hotspot *hotspot_list = NULL;
/* mutex needed */

struct hotspot {
	int fd;
	struct sockaddr_in remote;
	char rssi;
	struct hotspot *next;
};

void *hotspot_handler(void *arg)
{
	struct hotspot *hotspot, *hotspot_current = NULL, *hotspot_prev, *hotspot_next, *t;
	fd_set readfds;
	int nfds, ret;
#ifdef DEBUG
	unsigned int count;
#endif
	ssize_t size;
	struct timeval timeout;
	char rssi;

	/* arg ignored */

	while (1) {
		/* init */
		FD_ZERO(&readfds);

		pthread_mutex_lock(&mutex);
		hotspot = hotspot_list;
		pthread_mutex_unlock(&mutex);

#ifdef DEBUG
		count = 0;
#endif

		/* backup hotspot */
		hotspot_next = hotspot;
		for (nfds = -1; hotspot; hotspot = hotspot->next) {
			if (hotspot->fd > nfds)
				nfds = hotspot->fd;
			FD_SET(hotspot->fd, &readfds);
#ifdef DEBUG
			count++;
#endif
		}

#ifdef DEBUG
		printf("\n%u hotspots!!\n", count);
#endif

		memset(&timeout, 0, sizeof(timeout));
		timeout.tv_sec = 1;

		ret = select(++nfds, &readfds, NULL, NULL, &timeout);

		if (ret == -1) {
			perror("select");
			break;
		} else if (ret == 0) {
#ifdef DEBUG
			printf("Refresh hotspot connections\n");
#endif
			continue;
		}

		/* to see who sends new rssi value */
		for (hotspot = hotspot_next, hotspot_prev = NULL; hotspot;
				hotspot_prev = hotspot, hotspot = hotspot_next) {
			/* backup */
			hotspot_next = hotspot->next;

			if (FD_ISSET(hotspot->fd, &readfds)) {
				memset(buf, 0, sizeof(buf));
				size = recv(hotspot->fd, buf, sizeof(buf), 0);
				if (size < 0)
					perror("recv");
				/* remove the hotspot struct from hotspot_list */
				if (size <= 0) {
					if (!hotspot_prev) {
						pthread_mutex_lock(&mutex);
						if (hotspot != hotspot_list) {
							for (t = hotspot_list; t; t = t->next) {
								if (t->next == hotspot) {
									/* new hotspot_prev found */
									hotspot_prev = t;
									break;
								}
							}

							if (!t) {
								/* new hotspot_prev not found */
								perror("free");
								goto outer_break;
							}
						}
						pthread_mutex_unlock(&mutex);
					}

					if (hotspot_prev) {
						hotspot_prev->next = hotspot->next;
					} else {
						hotspot_list = hotspot->next;
					}

					/* reset hotspot_current */
					if (hotspot == hotspot_current)
						hotspot_current = NULL;

					close(hotspot->fd);
					free(hotspot);
					hotspot = hotspot_prev;

					continue;
				}
#ifdef DEBUG
				fprintf(stderr, "len = %d, %s\n", (int)size, buf);
#endif
				sscanf(buf, "%hhd", &rssi);

				if (rssi == 0)
					printf("--> NOT in Range from %s:%d\n",
							inet_ntoa(hotspot->remote.sin_addr),
							ntohs(hotspot->remote.sin_port));
				else
					printf("--> rssi = %hhd from %s:%d\n", rssi,
							inet_ntoa(hotspot->remote.sin_addr),
							ntohs(hotspot->remote.sin_port));

				hotspot->rssi = rssi;
			}
		}

		if (hotspot_current && hotspot_current->rssi == 0) {
			/* hotspot_current should disconnect */
			size = send(hotspot_current->fd,
					disconnect_message, sizeof(disconnect_message), 0);
			if (size < 0)
				perror("send to hotspot_current");

			/* reset hotspot_current */
			hotspot_current = NULL;
		}

		pthread_mutex_lock(&mutex);
		hotspot = hotspot_list;
		pthread_mutex_unlock(&mutex);

		if (hotspot_current && hotspot_current->rssi != 0)
			rssi = hotspot_current->rssi;
		else
			rssi = -11;
		/* to see who should connect to the device */
		for (hotspot_next = NULL; hotspot; hotspot = hotspot->next) {
			/* ignore the hotspot with NOT in Range */
			if (hotspot->rssi == 0)
				continue;

			if (hotspot->rssi > rssi) {
				/* the candidate */
				rssi = hotspot->rssi;
				hotspot_next = hotspot;
			}
		}

		if (hotspot_next) {
			if (hotspot_next != hotspot_current) {
				/* hotspot_next should connect */
				size = send(hotspot_next->fd, connect_message, sizeof(connect_message), 0);
				if (size < 0)
					perror("send to hotspot_next");
				if (size <= 0)
					continue;
				if (hotspot_current) {
					/* hotspot_current should disconnect */
					size = send(hotspot_current->fd,
							disconnect_message, sizeof(disconnect_message), 0);
					if (size < 0)
						perror("send to hotspot_current");
					if (size <= 0) {
						/* reset hotspot_current */
						hotspot_current = NULL;
						continue;
					}
				}

				/* new hotspot_current */
				hotspot_current = hotspot_next;
			}
		}
	}
outer_break:

	exit(errno);
}

int main(int argc, char *argv[])
{
	int fd, hotspot_fd, ret;
	struct sockaddr_in local, remote;
	socklen_t len;	
	pthread_t thread;
	struct hotspot *hotspot;
	ssize_t size;
	
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

	pthread_mutex_init(&mutex, NULL);
	pthread_create(&thread, NULL, hotspot_handler, NULL);

	do {
		len = sizeof(remote);
		hotspot_fd = accept(fd, (struct sockaddr *)&remote, &len);
		if (hotspot_fd < 0) {
			perror("accept");
			break;
		}
		size = send(hotspot_fd, connected_message, sizeof(connected_message), 0);
		if (size < 0)
			perror("send in main");

		hotspot = calloc(1, sizeof(*hotspot));
		if (hotspot == NULL) {
			perror("malloc");
			break;
		}
		hotspot->fd = hotspot_fd;
		hotspot->remote = remote;

		/* add to the hotspot list */
		pthread_mutex_lock(&mutex);
		hotspot->next = hotspot_list;
		hotspot_list = hotspot;
		pthread_mutex_unlock(&mutex);
	} while (1);

	pthread_mutex_destroy(&mutex);

	return errno;
}
