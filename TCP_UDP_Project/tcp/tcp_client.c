#ifdef _WIN32
	#define _WIN32_WINNT _WIN32_WINNT_WIN7
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>

	void OSInit(void)
	{
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 0), &wsaData) != 0)
		{
			fprintf(stderr, "WSAStartup failed\n");
			exit(-1);
		}
	}

	void OSCleanup(void)
	{
		WSACleanup();
	}

	#define CLOSESOCKET closesocket
	#define perror(msg) fprintf(stderr, msg ": %d\n", WSAGetLastError())

#else
	#include <sys/socket.h>
	#include <sys/types.h>
	#include <netdb.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <unistd.h>
	#include <errno.h>
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>

	void OSInit(void) {}
	void OSCleanup(void) {}

	#define CLOSESOCKET close
#endif

int initialization();
void execution(int server_socket);
void cleanup(int server_socket);

int main()
{
	OSInit();

	int server_socket = initialization();

	printf("Server gestart op poort 5500...\n");

	execution(server_socket);

	cleanup(server_socket);

	OSCleanup();

	return 0;
}

int initialization()
{
	struct addrinfo hints, *res;
	memset(&hints, 0, sizeof hints);

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if (getaddrinfo(NULL, "5500", &hints, &res) != 0)
	{
		perror("getaddrinfo");
		exit(1);
	}

	int server_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (server_socket == -1)
	{
		perror("socket");
		exit(1);
	}

	if (bind(server_socket, res->ai_addr, res->ai_addrlen) == -1)
	{
		perror("bind");
		exit(1);
	}

	if (listen(server_socket, 10) == -1)
	{
		perror("listen");
		exit(1);
	}

	freeaddrinfo(res);

	return server_socket;
}

void execution(int server_socket)
{
	fd_set master, read_fds;
	FD_ZERO(&master);
	FD_SET(server_socket, &master);

	int max_fd = server_socket;

	while (1)
	{
		read_fds = master;

		if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) == -1)
		{
			perror("select");
			exit(1);
		}

		for (int i = 0; i <= max_fd; i++)
		{
			if (FD_ISSET(i, &read_fds))
			{
				if (i == server_socket)
				{
					// nieuwe client
					struct sockaddr_storage client_addr;
					socklen_t addr_size = sizeof client_addr;

					int new_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_size);

					if (new_socket == -1)
					{
						perror("accept");
					}
					else
					{
						FD_SET(new_socket, &master);
						if (new_socket > max_fd)
							max_fd = new_socket;

						printf("Nieuwe client verbonden\n");
					}
				}
				else
				{
					// bericht ontvangen
					char buffer[1024];
					int bytes = recv(i, buffer, sizeof(buffer), 0);

					if (bytes <= 0)
					{
						// client disconnect
						printf("Client disconnected\n");
						CLOSESOCKET(i);
						FD_CLR(i, &master);
					}
					else
					{
						// broadcast naar andere clients
						for (int j = 0; j <= max_fd; j++)
						{
							if (FD_ISSET(j, &master) && j != server_socket && j != i)
							{
								send(j, buffer, bytes, 0);
							}
						}
					}
				}
			}
		}
	}
}

void cleanup(int server_socket)
{
	CLOSESOCKET(server_socket);
}