#define __CONNECTION_HPP__
#ifdef __CONNECTION_HPP__

#include <stdint.h>
#include <unistd.h>

int Socket(int domain, int type, int protocol);

void Bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

void Listen(int sockfd, int backlog);

void Connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

int Accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

void set_nonblocking(int fd);

void epoll_ctl_add(int epfd, int fd, uint32_t events);

void set_sockaddr(struct sockaddr_in *addr, int port);

#endif /* __CONNECTION_HPP__ */
