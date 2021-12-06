#ifndef PROXY_CLIENT_H 
#define PROXY_CLIENT_H

#include <string>
#include <memory>
#include <vector>
#include <list>
#include <mutex>
#include <thread>

#include <netdb.h> 

#include <dint/dint_public.h>

class Client
{
public:
    Client(const std::string &lname, int lport, const std::string &rname, int rport);
    ~Client();

    void start_socket();

    std::shared_ptr<std::vector<uint8_t>> read_msg();
    void write(const std::vector<uint8_t> &str);
    void writeln(const std::vector<uint8_t>  &str);
    void listen();
    void close();
    void connect();


private:
    void _writeln(const std::vector<uint8_t> &buf);
    void child_sighandler();
    void proxy_connection();

    sockaddr_storage m_addr;
    socklen_t m_addrlen;
    int m_lport;
    int m_rport;
    int m_listenfd;
    int m_sockfd;

    std::string m_lname;
    std::string m_rname;

    bool m_running = true;
    std::shared_ptr<std::list<pid_t>> m_childp;
    std::shared_ptr<std::mutex> m_mtx;
    std::shared_ptr<std::thread> m_sigthread;
};


#endif
