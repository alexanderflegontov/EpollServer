#include "connection.hpp"
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>

int Socket(int domain, int type, int protocol) {
  int ret = socket(domain, type, protocol);
  if (ret == -1) {
    perror("socket() failed");
    exit(EXIT_FAILURE);
  }
  return ret;
}

void Bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
  int ret = bind(sockfd, addr, addrlen);
  if (ret == -1) {
    perror("bind() failed");
    exit(EXIT_FAILURE);
  }
}

void Listen(int sockfd, int backlog) {
  int ret = listen(sockfd, backlog);
  if (ret == -1) {
    perror("listen() failed");
    exit(EXIT_FAILURE);
  }
}

void Connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
  int ret = connect(sockfd, addr, addrlen);
  if (ret == -1) {
    perror("connect() failed");
    exit(EXIT_FAILURE);
  }
}

int Accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
  int ret = accept(sockfd, addr, addrlen);
  if (ret < 0) {
    perror("accept() failed");
    exit(EXIT_FAILURE);
  }
  return ret;
}

void set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    perror("fcntl() failed");
    return;
  }
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    perror("fcntl() failed");
  }
}

void epoll_ctl_add(int epfd, int fd, uint32_t events) {
  struct epoll_event ev;
  ev.events = events;
  ev.data.fd = fd;
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
    perror("epoll_ctl() failed");
    exit(EXIT_FAILURE);
  }
}

void set_sockaddr(struct sockaddr_in *addr, int port) {
  addr->sin_family = AF_INET;
  addr->sin_addr.s_addr = INADDR_ANY;
  addr->sin_port = htons(port);
}
