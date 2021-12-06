#include <string>
#include <memory>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <regex>
#include <sstream>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h> 
#include <error.h>

#include <dint/dint_public.h>
#include <dint/exceptions.h>

#include "proxyclient.h"
#include "util.h"
#include "logger.h"
#include "tcppipe.h"

constexpr auto net_accept = accept;
constexpr auto net_listen = listen;

Client::Client(const std::string &lname, int lport, const std::string &rname, int rport)
    : m_lport (lport), m_rport(rport),
    m_lname (lname),  m_rname (rname),
    m_childp (nullptr)
{
    start_socket();
}

void Client::start_socket()
{
    addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // ipv4
    hints.ai_socktype = SOCK_STREAM; // tcp
    hints.ai_flags  = AI_PASSIVE; // use hostip

    const std::string port_ptr {std::to_string(m_lport)};

    const char *host = m_lname.c_str();
    addrinfo *res;
    int err = getaddrinfo(*host ? host : nullptr, port_ptr.c_str(), &hints, &res);
    if (err) {
        throw dint::Exception(gai_strerror(err));
    }

    // creat a socket with addr/sock type/and protocl
    m_listenfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, "1", sizeof(char*));

    // bind socket to port
    char name[INET6_ADDRSTRLEN];
    int port;
    sock_addrport(res, name, INET6_ADDRSTRLEN, &port);


    if (bind(m_listenfd, res->ai_addr, res->ai_addrlen)) {
        throw dint::Exception(strerror(errno));
    }

    // store sockaddr
    m_addrlen = sizeof m_addr;
    freeaddrinfo(res);

    if (getsockname(m_listenfd, (sockaddr*)&m_addr, &m_addrlen) == -1) {
        throw dint::Exception(strerror(errno));
    }
}

Client::~Client() {
}


void Client::child_sighandler()
{
    if (m_mtx == nullptr)
        return;

    int status;
    pid_t child;
    int prev_errno = 0; // errno is never zero
    while (m_running)
    {
        child = waitpid(-1, &status, 0);

        // check that a child process actually terminated
        if (!(WIFEXITED(status) || WIFSIGNALED(status))) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }


        if (child == -1 && prev_errno != errno)
        {
            LOGERR("child sighandler: %s\n", strerror(errno));
            prev_errno = errno;
        }
        else
        {
            std::lock_guard<std::mutex> guard(*m_mtx);
            // remove pid from child-pid list
            m_childp->remove(child);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

}

void Client::listen()
{

    char str_addr [INET6_ADDRSTRLEN ];
    int port;
    if(sock_addrport((sockaddr*)&m_addr, str_addr, INET6_ADDRSTRLEN, &port)) {
        throw dint::Exception(strerror(errno));
    }


    // listen for connections
    if (net_listen(m_listenfd, 5)) {
        throw dint::Exception(strerror(errno));
    }

    sockaddr_storage cl_addr;
    socklen_t cl_addr_size = sizeof cl_addr;

    bool stop = false;

    while (!stop)
    {
        LOGFC(tcolors::GREEN, "proxy client listening on: %s %i\n", str_addr, port);
        m_sockfd = net_accept(m_listenfd, (sockaddr*) &cl_addr, &cl_addr_size);

        // handle socket in a forked child process
        pid_t child = fork();
        if (child == -1)
        {
            throw dint::Exception(strerror(errno));
        }
        else if (child == 0)
        {

            if (m_sockfd < 0) {
                throw dint::Exception(strerror(errno));
            }

            sockaddr_storage peeraddr;
            socklen_t size = sizeof peeraddr;
            getpeername(m_sockfd, (sockaddr*)&peeraddr, &size);

            if(sock_addrport((sockaddr*)&peeraddr, str_addr, (size_t)INET6_ADDRSTRLEN, &port)) {
                throw dint::Exception(strerror(errno));
            }

            LOGF("child process: handling %s %i\n", str_addr, port);
            stop = true;
            proxy_connection();
        }
        else
        {
            if (m_childp == nullptr)
                m_childp = std::make_shared<std::list<pid_t>>();
            if (m_mtx == nullptr)
                m_mtx = std::make_shared<std::mutex>();

            {
                std::lock_guard<std::mutex> guard(*m_mtx);
                m_childp->push_back(child);
            }

            LOGF("parent: forked, current size %lu\n", m_childp->size());

            if (m_sigthread == nullptr) {
                m_sigthread = std::make_shared<std::thread>([&](){child_sighandler();});
            }
        }
    }
}


void Client::proxy_connection()
{
    try
    {
        dint::Client dint_client {m_rname, m_rport};
        // establish dint connection to proxy server
        dint_client.connect();

        TCPPipe pipe {m_sockfd};

        pipe.start_pipe(
                [&](){ return dint_client.read_msg(); },
                [&](std::shared_ptr<std::vector<uint8_t>> buf){ dint_client.writeln(*buf); },
                [&](){ dint_client.poll_sig = true; dint_client.close_sock(); },
                [&](){ dint_client.poll_sig = true; dint_client.close_sock(); }
            );
    }
    catch (const std::exception &e)
    {
        LOGERR("proxy connection: %s\n", e.what());
    }
}
