#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <netdb.h>

#include "link.h"
#include "link_addr.h"

namespace redis {

Link::Link() {
    sock = -1;
    noblock_ = false;
    remote_ip[0] = '\0';
    remote_port = -1;
}

Link::~Link() {
    this->close();
}

void Link::close() {
    if (sock >= 0) {
        ::close(sock);
    }
}

void Link::nodelay(bool enable) {
    int opt = enable ? 1 : 0;
    ::setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (void*)&opt, sizeof(opt));
}

void Link::keepalive(bool enable) {
    int opt = enable ? 1 : 0;
    ::setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (void*)&opt, sizeof(opt));
}

void Link::noblock(bool enable) {
    noblock_ = enable;
    if (enable) {
        ::fcntl(sock, F_SETFL, O_NONBLOCK | O_RDWR);
    } else {
        ::fcntl(sock, F_SETFL, O_RDWR);
    }
}

static bool is_ip(const char* host) {
    if (strchr(host, ':') != NULL) {
        return true;
    }
    int dot_count = 0;
    int digit_count = 0;
    for (const char* p = host; *p; p++) {
        if (*p == '.') {
            dot_count += 1;
            if (digit_count >= 1 && digit_count <= 3) {
                digit_count = 0;
            } else {
                return false;
            }
        } else if (*p >= '0' && *p <= '9') {
            digit_count += 1;
        } else {
            return false;
        }
    }
    return dot_count == 3;
}

Link* Link::connect(const char* host, int port) {
    Link* link;
    int sock = -1;

    char ip_resolve[INET6_ADDRSTRLEN];
    if (!is_ip(host)) {
        struct hostent* hptr = gethostbyname(host);
        for (int i = 0; hptr && hptr->h_addr_list[i] != NULL; i++) {
            struct in_addr* addr = (struct in_addr*)hptr->h_addr_list[i];
            if (inet_ntop(AF_INET, addr, ip_resolve, sizeof(ip_resolve))) {
                //printf("resolve %s: %s\n", host, ip_resolve);
                host = ip_resolve;
                break;
            }
        }
    }

    LinkAddr addr(host, port);

    if ((sock = ::socket(addr.family, SOCK_STREAM, 0)) == -1) {
        goto sock_err;
    }
    if (::connect(sock, addr.addr(), addr.addrlen) == -1) {
        goto sock_err;
    }

    //log_debug("fd: %d, connect to %s:%d", sock, ip, port);
    link = new Link();
    link->ipv4 = addr.ipv4;
    link->sock = sock;
    link->keepalive(true);
    link->nodelay(true);
    return link;
sock_err:
    //log_debug("connect to %s:%d failed: %s", ip, port, strerror(errno));
    if (sock >= 0) {
        ::close(sock);
    }
    return NULL;
}

Link* Link::listen(const char* ip, int port) {
    Link* link;
    int sock = -1;
    LinkAddr addr(ip, port);

    int opt = 1;
    if ((sock = ::socket(addr.family, SOCK_STREAM, 0)) == -1) {
        goto sock_err;
    }
    if (::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        goto sock_err;
    }
    if (::bind(sock, addr.addr(), addr.addrlen) == -1) {
        goto sock_err;
    }
    if (::listen(sock, 1024) == -1) {
        goto sock_err;
    }
    //log_debug("server socket fd: %d, listen on: %s:%d", sock, ip, port);

    link = new Link();
    link->ipv4 = addr.ipv4;
    link->sock = sock;
    snprintf(link->remote_ip, sizeof(link->remote_ip), "%s", ip);
    link->remote_port = port;
    return link;
sock_err:
    //log_debug("listen %s:%d failed: %s", ip, port, strerror(errno));
    if (sock >= 0) {
        ::close(sock);
    }
    return NULL;
}

Link* Link::accept() {
    Link* link;
    int client_sock;
    LinkAddr addr(this->ipv4);

    while ((client_sock = ::accept(sock, addr.addr(), &addr.addrlen)) == -1) {
        if (errno != EINTR) {
            //log_error("socket %d accept failed: %s", sock, strerror(errno));
            return NULL;
        }
    }

    // avoid client side TIME_WAIT
    struct linger opt = {1, 0};
    int ret = ::setsockopt(client_sock, SOL_SOCKET, SO_LINGER, (void*)&opt, sizeof(opt));
    if (ret != 0) {
        //log_error("socket %d set linger failed: %s", client_sock, strerror(errno));
    }

    link = new Link();
    link->sock = client_sock;
    link->keepalive(true);
    link->nodelay(true);
    link->remote_port = addr.port();
    inet_ntop(addr.family, addr.sin_addr(), link->remote_ip, sizeof(link->remote_ip));
    if (!addr.ipv4) {
        if (strchr(link->remote_ip, '.')) {
            char* p = strrchr(link->remote_ip, ':');
            if (p) {
                p += 1;
                char* s = link->remote_ip;
                while (*p != '\0') {
                    *s = *p;
                    s++;
                    p++;
                }
                *s = '\0';
            }
        }
    }
    return link;
}

int Link::read() {
    int ret = 0;
    char buf[16 * 1024];
    int want = sizeof(buf);
    while (1) {
        // test
        //want = 1;
        int len = ::read(sock, buf, want);
        if (len == 0) {
            return -1;
        } else if (len == -1) {
            if (errno == EINTR) {
                continue;
            } else if (errno == EWOULDBLOCK) {
                break;
            } else {
                //log_debug("fd: %d, read: -1, want: %d, error: %s", sock, want, strerror(errno));
                return -1;
            }
        } else {
            //log_debug("fd: %d, want=%d, read: %d", sock, want, len);
            ret += len;
            recv_buf.append(buf, len);
        }
        break;
    }
    //log_debug("read %d", ret);
    return ret;
}

int Link::recv(Message* req) {
    while (1) {
        int n = req->Decode(recv_buf);
        if (n == 0) {
            if (noblock_) {
                break;
            }
            int ret = this->read();
            if (ret == 0) {
                return 0;
            } else if (ret == -1) {
                return -1;
            }
        } else if (n == -1) {
            return -1;
        } else {
            recv_buf = recv_buf.substr(n);
            return n;
        }
    }
    return 0;
}

int Link::write() {
    int ret = 0;
    while (ret < (int)send_buf.size()) {
        int want = send_buf.size() - ret;
        int len = ::write(sock, send_buf.data() + ret, want);
        if (len == -1) {
            if (errno == EINTR) {
                continue;
            } else if (errno == EWOULDBLOCK) {
                break;
            } else {
                return -1;
            }
        } else {
            ret += len;
        }
        break;
    }
    if (ret > 0) {
        send_buf = send_buf.substr(ret);
    }
    return ret;
}

int Link::send(const Response& resp) {
    std::string data = resp.Encode();
    send_buf.append(data);
    if (!noblock_) {
        while (1) {
            int ret = this->write();
            if (ret == 0) { // would block
                break;
            } else if (ret == -1) {
                return -1;
            }
            if (send_buf.empty()) {
                break;
            }
        }
    }
    return (int)data.size();
}

}; // namespace redis
