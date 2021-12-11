#include <string>
#include <memory>
#include <sstream>
#include <vector>
#include <iostream>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <thread>
#include <mutex>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h> 
#include <error.h>
#include <fcntl.h>

#include <dint/dint_public.h>
#include <logger.h>
#include "tcppipe.h"
#include "util.h"

constexpr auto net_connect = connect;

TCPPipe::TCPPipe(const std::string &name, int port)
    : m_name (name), m_port (port), m_addr ()
{
    start_socket();
    connect();
}

TCPPipe::TCPPipe(int sockfd)
    : m_sockfd (sockfd), m_addr ()
{
    char name[INET6_ADDRSTRLEN];

    m_addrlen = sizeof m_addr;
    if (getsockname(m_sockfd, (sockaddr*)&m_addr, &m_addrlen) == -1) {
        throw dint::Exception(strerror(errno));
    }

    if(sock_addrport((sockaddr*)&m_addr, name, INET6_ADDRSTRLEN, &m_port)) {
        throw dint::Exception(strerror(errno));
    }

    m_name = std::string(name);

    // set socketfd as non-blocking
    int flags = fcntl(m_sockfd, F_GETFL, 0);
    fcntl(m_sockfd, F_SETFL, flags | O_NONBLOCK);

    LOGF("using existing socket %s:%i\n", name, m_port);
}


TCPPipe::~TCPPipe() 
{
    if (m_sockfd)
        shutdown(m_sockfd, 2);
}


void TCPPipe::start_socket()
{
    addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;     // ipv4 or ipv6
    hints.ai_socktype = SOCK_STREAM; // tcp
    hints.ai_flags  = AI_PASSIVE;    // use hostip

    const std::string port_ptr {std::to_string(m_port)};

    addrinfo *res;
    int err = getaddrinfo(m_name.c_str(), port_ptr.c_str(), &hints, &res);
    if (err) {
        throw dint::Exception(gai_strerror(err));
    }

    // create a socket with addr/sock type/and protocl
    m_sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    m_addrinfo = res;
}


void TCPPipe::connect()
{
    // create socket connection to remote host
    char str_addr [INET6_ADDRSTRLEN ];
    int port;
    if(sock_addrport(m_addrinfo, str_addr, INET6_ADDRSTRLEN, &port)) {
        throw dint::Exception(strerror(errno));
    }

    LOGF("connecting to %s:%i\n", str_addr, port);
    m_name = str_addr;
    m_port = port;

    int err = net_connect(m_sockfd, m_addrinfo->ai_addr, m_addrinfo->ai_addrlen);
    if (err) {
        freeaddrinfo(m_addrinfo);
        m_addrinfo = nullptr;
        throw dint::Exception(strerror(errno));
    }

    // set socketfd as non-blocking
    int flags = fcntl(m_sockfd, F_GETFL, 0);
    fcntl(m_sockfd, F_SETFL, flags | O_NONBLOCK);

    m_addrlen = sizeof m_addr;
    err = getsockname(m_sockfd, (sockaddr*)&m_addr, &m_addrlen);
    freeaddrinfo(m_addrinfo);
    m_addrinfo = nullptr;


    if(err == -1) {
        throw dint::Exception(gai_strerror(err));
    }
}


void TCPPipe::reader(const send_f &send)
{
    auto buf = std::make_shared<std::vector<uint8_t>>();
    buf->resize(READ_BUFFER_SIZE);
    // buf->reserve(READ_BUFFER_SIZE);

    int read = 0;;
    while (!m_poll_sig)
    {
        read = poll_recv(m_sockfd, (char*)buf->data(), READ_BUFFER_SIZE, 0.1f, &m_poll_sig);

        // resize buffer to fit bytes that were read in
        if (read > 0) {
            buf->resize(read);
            LOGF("reader read from %s:%i, sending\n", m_name.c_str(), m_port);
            LOGBUF(*buf);

            send(buf);
        }
    }
    if (m_poll_sig)
        LOGERR("reader thread: stopped polling\n");
}


void TCPPipe::writer(const recv_f &recv)
{
    std::shared_ptr<std::vector<uint8_t>> buf;
    int sent = 0, err = 0;
    size_t len = 0;
    while (!m_poll_sig)
    {
        buf = recv();
        sent = 0;
        len = buf->size();

        if (len > 0) {
            LOGF("writer read, sending to %s:%i\n", m_name.c_str(), m_port);
            LOGBUF(*buf);
        }

        while (sent < len) {
            err = send(m_sockfd, buf->data()+sent, len-sent, 0);

            if (err == -1) {
                throw dint::Exception(strerror(errno));
            } else {
                sent += err;
            }
        }
    }
    if (m_poll_sig)
        LOGERR("writer thread: stopped polling\n");
}

void TCPPipe::start_pipe(const recv_f &receive, const send_f &send,
        const std::function<void()> &rquit, const std::function<void()> &squit)
{
    m_poll_sig = false;
    std::mutex mtx;
    std::thread reader_thread ([&](){
            try{ reader(send); }
            catch(const std::exception &e) {
                LOGERR("reader thread: %s\n", e.what());
                m_poll_sig = true;
            }

            try {
                if (rquit) {
                // only one thread cleanup at one time
                    std::lock_guard<std::mutex> l {mtx};
                    rquit();
                };
            } catch(const std::exception &e) {
                LOGERR("reader thread error quitting: %s\n", e.what());
            }
        });
    std::thread writer_thread ([&](){
            try{ 
                writer(receive); 
            }
            catch(const std::exception &e) {
                LOGERR("writer thread: %s\n", e.what());
                m_poll_sig = true;
            }

            try {
                if (squit) {
                // only one thread cleanup at one time
                    std::lock_guard<std::mutex> l {mtx};
                    squit();
                }
            } catch(const std::exception &e) {
                LOGERR("writer thread error quitting: %s\n", e.what());
            }
        });

    reader_thread.join();
    writer_thread.join();
}
