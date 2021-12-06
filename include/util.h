#ifndef PROXY_UTIL_H
#define PROXY_UTIL_H

#include <netdb.h> 

int sock_addrport(addrinfo *addr, char *str, size_t len, int *port);
int sock_addrport(sockaddr *addr, char *str, size_t len, int *port);
int poll_recv(int sockfd, char *ptr, size_t length, double timeout = -1.0f, bool *signal = nullptr);

#endif
