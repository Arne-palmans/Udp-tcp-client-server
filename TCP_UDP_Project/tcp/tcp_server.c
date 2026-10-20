#ifdef _WIN32
	#define _WIN32_WINNT _WIN32_WINNT_WIN7
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
	#include <io.h>
	#include <conio.h>
	#pragma comment(lib, "Ws2_32.lib")
	void OSInit(void) {
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
			fprintf(stderr, "WSAStartup failed\n");
			exit(EXIT_FAILURE);
		}
	}
	void OSCleanup(void) {
		WSACleanup();
	}
	#define close closesocket
#else
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netdb.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <unistd.h>
	#include <errno.h>
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
	#include <signal.h>
	void OSInit(void) {}
	void OSCleanup(void) {}
#endif

#define DEFAULT_PORT "5800"
#define MAX_CLIENTS 64
#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
	OSInit();
	const char *port = (argc >= 2) ? argv[1] : DEFAULT_PORT;
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int listen_socket = -1;
	int optval = 1;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if (getaddrinfo(NULL, port, &hints, &result) != 0) {
		perror("getaddrinfo");
		OSCleanup();
		return EXIT_FAILURE;
	}

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		listen_socket = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (listen_socket < 0) continue;

		setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, (const char *)&optval, sizeof(optval));

		if (bind(listen_socket, rp->ai_addr, rp->ai_addrlen) == 0) break;
		printf("Bind gelukt\n");

		close(listen_socket);
	}

	freeaddrinfo(result);

	if (rp == NULL) {
		fprintf(stderr, "Could not bind to port %s\n", port);
		OSCleanup();
		return EXIT_FAILURE;
	}

	if (listen(listen_socket, SOMAXCONN) != 0) {
		perror("listen");
		close(listen_socket);
		OSCleanup();
		return EXIT_FAILURE;
	}

	printf("Listening...\n");
	printf("Chatroom gestart op poort %s\n", port);

	fd_set master_set, read_set;
	FD_ZERO(&master_set);
	FD_SET(listen_socket, &master_set);
	int max_fd = listen_socket;

	while (1) {
		read_set = master_set;
		if (select(max_fd + 1, &read_set, NULL, NULL, NULL) < 0) {
			perror("select");
			break;
		}

		for (int fd = 0; fd <= max_fd; fd++) {
			if (!FD_ISSET(fd, &read_set)) continue;

			if (fd == listen_socket) {
				struct sockaddr_storage client_addr;
				socklen_t addr_len = sizeof client_addr;
				int new_socket = accept(listen_socket, (struct sockaddr*)&client_addr, &addr_len);
				if (new_socket < 0) {
					perror("accept");
					continue;
				}

				char host[NI_MAXHOST], svc[NI_MAXSERV];
				getnameinfo((struct sockaddr*)&client_addr, addr_len, host, sizeof host, svc, sizeof svc, NI_NUMERICHOST | NI_NUMERICSERV);
				printf("Nieuwe client: %s:%s (socket %d)\n", host, svc, new_socket);

				FD_SET(new_socket, &master_set);
				if (new_socket > max_fd) max_fd = new_socket;
				continue;
			}

			char buffer[BUFFER_SIZE];
			int bytes_received = recv(fd, buffer, BUFFER_SIZE - 1, 0);
			if (bytes_received <= 0) {
				if (bytes_received == 0) {
					printf("Client %d gesloten verbinding.\n", fd);
				} else {
					perror("recv");
				}
				close(fd);
				FD_CLR(fd, &master_set);
			} else {
				buffer[bytes_received] = '\0';
				char msg[BUFFER_SIZE + 64];
				snprintf(msg, sizeof msg, "Client %d: %s", fd, buffer);

				for (int jd = 0; jd <= max_fd; jd++) {
					if (FD_ISSET(jd, &master_set) && jd != listen_socket && jd != fd) {
						send(jd, msg, (int)strlen(msg), 0);
					}
				}
			}
		}
	}

	close(listen_socket);
	OSCleanup();
	return EXIT_SUCCESS;
}
