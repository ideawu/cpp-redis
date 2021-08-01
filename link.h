#ifndef NET_LINK_H_
#define NET_LINK_H_

#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "Message.h"
#include "Response.h"

namespace redis {

class Link {
private:
    int sock;
    bool noblock_;
    bool ipv4;
    std::string recv_buf;
    std::string send_buf;

public:
    char remote_ip[INET6_ADDRSTRLEN];
    int remote_port;

    Link();
    ~Link();
    int fd() const {
        return sock;
    }
    void close();
    void nodelay(bool enable = true);
    // noblock(true) is supposed to corperate with IO Multiplex,
    // otherwise, flush() may cause a lot unneccessary write calls.
    void noblock(bool enable = true);
    void keepalive(bool enable = true);

    static Link* connect(const char* ip, int port);
    static Link* listen(const char* ip, int port);
    Link* accept();

    int read();
    int write();

    // 0: not ready, -1: error
    int recv(Message* req);
    int send(const Response& resp);
};

}; // namespace redis

#endif
