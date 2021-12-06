#ifndef PROXY_TCP_PIPE_H 
#define PROXY_TCP_PIPE_H

#include <string>
#include <memory>
#include <vector>
#include <functional>

#include <netdb.h> 

#include <dint/dint_public.h>

typedef std::function<void(std::shared_ptr<std::vector<uint8_t>>)> send_f;
typedef std::function<std::shared_ptr<std::vector<uint8_t>>(void)> recv_f;

class TCPPipe
{
public:
    TCPPipe(const std::string &name, int port);
    TCPPipe(int sockfd);
    ~TCPPipe();
    std::shared_ptr<std::vector<uint8_t>> read_msg();

    void start_pipe(const recv_f &recv, const send_f &send,
        const std::function<void()> &rquit, const std::function<void()> &squit);


private:
    void reader(const send_f &send);
    void writer(const recv_f &recv);

    void start_socket();
    void connect();

    addrinfo *m_addrinfo;
    sockaddr_storage m_addr;
    socklen_t m_addrlen;

    size_t READ_BUFFER_SIZE = 8192;
    bool m_poll_sig = false;

    int m_port;
    int m_sockfd;
    std::string m_name;
};


#endif
