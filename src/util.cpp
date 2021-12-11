#include <thread>
#include <chrono>
#include <algorithm>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h> 
#include <time.h>
#include <error.h>


#include <dint/dint_public.h>
#include "util.h"

int sock_addrport(addrinfo *addr, char *str, size_t len, int *port)
{
    void *sin_addr = addr->ai_addr;
    if (addr->ai_family == AF_INET) {
        sin_addr = &((sockaddr_in*)addr->ai_addr)->sin_addr;
        *port    =  ntohs(((sockaddr_in*)addr->ai_addr)->sin_port);
    } else if (addr->ai_family == AF_INET6) {
        sin_addr = &((sockaddr_in6*)addr->ai_addr)->sin6_addr;
        *port    =  ntohs(((sockaddr_in6*)addr->ai_addr)->sin6_port);
    }
    const char *ptr = inet_ntop(addr->ai_family, sin_addr, str, len);

    return !ptr;
}

int sock_addrport(sockaddr *addr, char *str, size_t len, int *port)
{
    void *sin_addr = addr;
    if (addr->sa_family == AF_INET) {
        sin_addr = &((sockaddr_in*)addr)->sin_addr;
        *port    =  ntohs(((sockaddr_in*)addr)->sin_port);
    } else if (addr->sa_family == AF_INET6) {
        sin_addr = &((sockaddr_in6*)addr)->sin6_addr;
        *port    =  ntohs(((sockaddr_in6*)addr)->sin6_port);
    }

    const char *ptr = inet_ntop(addr->sa_family, sin_addr, str, len);

    return !ptr;
}

int poll_recv(int sockfd, char *ptr, size_t length, double timeout, bool *signal)
{
    time_t start = time(0);
    const bool ignore = (timeout < 0.0f);

    int read = 0;
    int err = 0; 
    while (read < length) {
        err = recv(sockfd, ptr+read, length-read, 0);

        if (err < 0) 
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // would have blocked if socket was not O_NONBLOCK
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                if ((!ignore && timeout <= difftime(time(0), start)) || (signal && *signal))
                    return read;
                std::chrono::high_resolution_clock::now();
                continue;
            } else if (read > 0)
            {
                return read;
            }

            throw dint::Exception(strerror(errno));
        }
        else if (err == 0) {
            // socket got closed
            if (read <= 0)
                throw dint::Exception("closed while reading");
            else
                return read;
        }
        else {
            // ok
            read += err;
        }
    }

    return read;
}

