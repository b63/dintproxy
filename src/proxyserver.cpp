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

#include "proxyserver.h"
#include "util.h"
#include "tcppipe.h"
#include "logger.h"

constexpr auto net_accept = accept;
constexpr auto net_listen = listen;

Server::Server(const std::string &name, int port)
    : m_port (port),
    m_name (name), m_childp (nullptr)
{
    start_socket();
}

void Server::start_socket()
{
    addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // ipv4
    hints.ai_socktype = SOCK_STREAM; // tcp
    hints.ai_flags  = AI_PASSIVE; // use hostip

    const std::string port_ptr {std::to_string(m_port)};

    const char *host = m_name.c_str();
    addrinfo *res;
    int err = getaddrinfo(*host ? host : nullptr, port_ptr.c_str(), &hints, &res);
    if (err) {
        throw dint::Exception(gai_strerror(err));
    }

    // creat a socket with addr/sock type/and protocl
    m_listenfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, "1", sizeof(char*));

    // bind socket to port
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

Server::~Server() {
}


void Server::child_sighandler()
{
    if (m_mtx == nullptr)
        return;

    int status;
    pid_t child;
    int prev_errno = 0;
    while (m_running)
    {
        child = waitpid(-1, &status, 0);

        // check that a child process actually terminated
        if (!(WIFEXITED(status) || WIFSIGNALED(status))) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        if (child == -1 && errno != prev_errno)
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

void Server::listen()
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
        LOGFC(tcolors::GREEN, "proxy server listening on %s:%i ..\n", str_addr, port);
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

            if(sock_addrport((sockaddr*)&peeraddr, str_addr, (size_t)INET6_ADDRSTRLEN, &port))
            {
                throw dint::Exception(strerror(errno));
            }

            LOGF("child process handling %s:%i\n", str_addr, port);
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

void Server::proxy_connection()
{
      try
      {
        dint::Server dint_server {m_sockfd};
        // read the initial message
        auto buf = std::make_shared<std::vector<uint8_t>>();
        auto rem = std::make_shared<std::vector<uint8_t>>();

        // TODO: OH MY GOD, FIGURE OUT SOMETHING ELSE THIS IS HORRIBLE
        bool end = false;
        while (!end) // keep reading till \r\n\r\n is encountered ...
        {
            auto temp {dint_server.read_msg()};
            buf->insert(buf->end(), temp->begin(), temp->end());

            auto start = buf->end() - temp->size() - 4;
            while (1)
            {
                auto it = std::find(start, buf->end(), '\r');
                if (it == buf->end()) break;
                if (it + 4 <= buf->end()) {
                    if (!strncmp((char*)&(*it), "\r\n\r\n", 4)) {
                        end = true;
                        if (it+4 < buf->end()) {
                            rem->insert(rem->end(), it+4, buf->end());
                            buf->erase(it+3, buf->end());
                        }
                        break;
                    }
                } else { break; }
                start = it + 1;
            }

        }

        size_t n = buf->size();

        // expect CONNECT header
        if (n <= 7 || strncmp((char*)buf->data(), "CONNECT", 7)) {
            throw dint::Exception("expected CONNECT header");
        }
        // read the CONNECT header from first 
        std::string header ((const char*)buf->data(), n);
        std::regex rgx ("^CONNECT\\s+(.+):(\\d+)\\s+HTTP/(.+)\r\n");
        std::cmatch cm;
        std::regex_search(header.c_str(), cm, rgx);

        if (cm.size() < 4) {
            throw dint::Exception("malformed header");
        }

        std::string host = cm.str(1);
        int port = std::atoi(cm.str(2).c_str());
        TCPPipe pipe {host, port};

        // send succesfull connecton mesage before starting pipe
        // TODO: change after modifying dint::Server to accept strings
        std::stringstream ss;
        ss << "HTTP/" << cm.str(3) << " 200 OK\r\n\r\n";
        std::string str {ss.str()};
        buf->resize(str.size());
        memcpy(buf->data(), str.c_str(), str.size());

        dint_server.writeln(*buf);

        pipe.start_pipe(
                [&]()
                { 
                    if (rem) {
                        auto t = rem;
                        rem = nullptr;
                        return t;
                    }
                    return dint_server.read_msg(); 
                },
                [&](std::shared_ptr<std::vector<uint8_t>> buf){ dint_server.writeln(*buf); },
                [&](){ dint_server.poll_sig = true; dint_server.close_sock(); },
                [&](){ dint_server.poll_sig = true; dint_server.close_sock(); }
            );

    }
    catch (const std::exception &e)
    {
        LOGERR("proxy connection: %s\n", e.what());
    }
}
