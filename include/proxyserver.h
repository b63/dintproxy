#ifndef PROXY_SERVER_H 
#define PROXY_SERVER_H

#include <string>
#include <memory>
#include <vector>
#include <list>
#include <thread>
#include <mutex>

#include <dint/dint_public.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h> 

class Server
{
public:
    Server(const std::string &name, int port);
    ~Server();
    void write(const std::string &str);
    void writeln(const std::vector<uint8_t> &buf);
    std::shared_ptr<std::vector<uint8_t>> read_msg();
    void flush();
    void close();
    void listen();

private:
    void proxy_connection();
    void child_sighandler();
    void start_socket();

    sockaddr_storage m_addr;
    socklen_t m_addrlen;
    int m_listenfd;
    int m_sockfd;
    int m_port;
    std::string m_name;

    bool m_running = true;
    std::shared_ptr<std::list<pid_t>> m_childp;
    std::shared_ptr<std::mutex> m_mtx;
    std::shared_ptr<std::thread> m_sigthread;

    void _writeln(const std::vector<uint8_t> &buf);
};


#endif
