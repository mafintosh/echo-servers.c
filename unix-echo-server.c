#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define BUFFER_SIZE 1024
#define on_error(...) { fprintf(stderr, __VA_ARGS__); fflush(stderr); exit(1); }

int main (int argc, char *argv[]) {
  if (argc < 2) on_error("Usage: %s [socket-path]\n", argv[0]);

  char *sock_path = argv[1];

  int server_fd, client_fd, err;
  struct sockaddr_un server, client;
  char buf[BUFFER_SIZE];

  server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server_fd < 0) on_error("Could not create socket\n");

  server.sun_family = AF_UNIX;
  strncpy(server.sun_path, sock_path, strlen(sock_path)+1);
  unlink(server.sun_path);

  err = bind(server_fd, (struct sockaddr *) &server, sizeof(server));
  if (err < 0) on_error("Could not bind socket\n");

  err = listen(server_fd, 128);
  if (err < 0) on_error("Could not listen on socket\n");

  printf("Server is listening on %s\n", sock_path);

  while (1) {
    socklen_t client_len = sizeof(client);

    client_fd = accept(server_fd, (struct sockaddr *) &client, &client_len);
    if (client_fd < 0) on_error("Could not establish new connection\n");

    while (1) {
      int read = recv(client_fd, buf, BUFFER_SIZE, 0);

      if (!read) break; // done reading
      if (read < 0) on_error("Client read failed\n");

      err = send(client_fd, buf, read, 0);
      if (err < 0) on_error("Client write failed\n");
    }
  }

  return 0;
}
