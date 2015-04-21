#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <errno.h>

#define BUFFER_SIZE 1024
#define on_error(...) { fprintf(stderr, __VA_ARGS__); fflush(stderr); exit(1); }

static struct kevent *events;
static int events_used = 0;
static int events_alloc = 0;

static struct sockaddr_in server;
static int server_fd, queue;

struct event_data {
  char buffer[BUFFER_SIZE];
  int buffer_read;
  int buffer_write;

  int (*on_read) (struct event_data *self, struct kevent *event);
  int (*on_write) (struct event_data *self, struct kevent *event);
};

static void event_server_listen (int port) {
  int err, flags;

  queue = kqueue();
  if (queue < 0) on_error("Could not create kqueue: %s\n", strerror(errno));

  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) on_error("Could not create server socket: %s\n", strerror(errno))

  server.sin_family = AF_INET;
  server.sin_port = htons(port);
  server.sin_addr.s_addr = htonl(INADDR_ANY);

  int opt_val = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));

  err = bind(server_fd, (struct sockaddr *) &server, sizeof(server));
  if (err < 0) on_error("Could not bind server socket: %s\n", strerror(errno));

  flags = fcntl(server_fd, F_GETFL, 0);
  if (flags < 0) on_error("Could not get server socket flags: %s\n", strerror(errno))

  err = fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);
  if (err < 0) on_error("Could set server socket to be non blocking: %s\n", strerror(errno));

  err = listen(server_fd, SOMAXCONN);
  if (err < 0) on_error("Could not listen: %s\n", strerror(errno));
}

static void event_change (int ident, int filter, int flags, void *udata) {
  struct kevent *e;

  if (events_alloc == 0) {
    events_alloc = 64;
    events = malloc(events_alloc * sizeof(struct kevent));
  }
  if (events_alloc <= events_used) {
    events_alloc *= 2;
    events = realloc(events, events_alloc * sizeof(struct kevent));
  }

  int index = events_used++;
  e = &events[index];

  e->ident = ident;
  e->filter = filter;
  e->flags = flags;
  e->fflags = 0;
  e->data = 0;
  e->udata = udata;
}

static void event_loop () {
  int new_events;

  while (1) {
    new_events = kevent(queue, events, events_used, events, events_alloc, NULL);
    if (new_events < 0) on_error("Event loop failed: %s\n", strerror(errno));
    events_used = 0;

    for (int i = 0; i < new_events; i++) {
      struct kevent *e = &events[i];
      struct event_data *udata = (struct event_data *) e->udata;

      if (udata == NULL) continue;
      if (udata->on_write != NULL && e->filter == EVFILT_WRITE) while (udata->on_write(udata, e));
      if (udata->on_read != NULL && e->filter == EVFILT_READ) while (udata->on_read(udata, e));
    }
  }
}

static int event_flush_write (struct event_data *self, struct kevent *event) {
  int n = write(event->ident, self->buffer + self->buffer_write, self->buffer_read - self->buffer_write);

  if (n < 0) {
    if (errno == EWOULDBLOCK || errno == EAGAIN) return 0;
    on_error("Write failed (should this be fatal?): %s\n", strerror(errno));
  }

  if (n == 0) {
    free(self);
    close(event->ident);
    return 0;
  }

  self->buffer_write += n;

  if (self->buffer_write == self->buffer_read) {
    self->buffer_read = 0;
    self->buffer_write = 0;
  }

  return 0;
}

static int event_on_read (struct event_data *self, struct kevent *event) {
  if (self->buffer_read == BUFFER_SIZE) {
    event_change(event->ident, EVFILT_READ, EV_DISABLE, self);
    return 0;
  }

  int n = read(event->ident, self->buffer + self->buffer_read, BUFFER_SIZE - self->buffer_read);

  if (n < 0) {
    if (errno == EWOULDBLOCK || errno == EAGAIN) return 0;
    on_error("Read failed (should this be fatal?): %s\n", strerror(errno));
  }

  if (n == 0) {
    free(self);
    close(event->ident);
    return 0;
  }

  if (self->buffer_read == 0) {
    event_change(event->ident, EVFILT_WRITE, EV_ENABLE, self);
  }

  self->buffer_read += n;
  return event_flush_write(self, event);
}

static int event_on_write (struct event_data *self, struct kevent *event) {
  if (self->buffer_read == self->buffer_write) {
    event_change(event->ident, EVFILT_WRITE, EV_DISABLE, self);
    return 0;
  }

  return event_flush_write(self, event);
}

static int event_on_accept (struct event_data *self, struct kevent *event) {
  struct sockaddr client;
  socklen_t client_len = sizeof(client);

  int client_fd = accept(server_fd, &client, &client_len);
  int flags;
  int err;

  if (client_fd < 0) {
    if (errno == EWOULDBLOCK || errno == EAGAIN) return 0;
    on_error("Accept failed (should this be fatal?): %s\n", strerror(errno));
  }

  flags = fcntl(client_fd, F_GETFL, 0);
  if (flags < 0) on_error("Could not get client socket flags: %s\n", strerror(errno));

  err = fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
  if (err < 0) on_error("Could not set client socket to be non blocking: %s\n", strerror(errno));

  struct event_data *client_data = (struct event_data *) malloc(sizeof(struct event_data));
  client_data->on_read = event_on_read;
  client_data->on_write = event_on_write;

  event_change(client_fd, EVFILT_READ, EV_ADD | EV_ENABLE, client_data);
  event_change(client_fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, client_data);

  return 1;
}

int main (int argc, char *argv[]) {
  if (argc < 2) {
    printf("Usage: %s [port]\n", argv[0]);
    exit(1);
  }

  int port = atoi(argv[1]);

  struct event_data server = {
    .on_read = event_on_accept,
    .on_write = NULL
  };

  event_server_listen(port);
  event_change(server_fd, EVFILT_READ, EV_ADD | EV_ENABLE, &server);
  event_loop();

  return 0;
}
