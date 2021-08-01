#include "Transport.h"
#include <errno.h>
#include <string.h>
#include "link.h"
#include "Channel.h"
#include "fde.h"

namespace redis {

Transport::Transport() {
    _id_incr = 1;
    _serv_link = NULL;
    _recv_channel = new Channel<Message>();
    _close_flag = false;
}

Transport::~Transport() {
    if (_main_thread.joinable()) {
        _close_flag = true;
        _main_thread.join();
    }

    delete _serv_link;
    delete _recv_channel;
}

int Transport::Start(const std::string& ip, int port) {
    _serv_link = Link::listen(ip.c_str(), port);
    if (!_serv_link) {
        fprintf(stderr, "listen %s:%d failed: %s\n", ip.c_str(), port, strerror(errno));
        return -1;
    }

    const int NUM = 4;
    accept_queues.resize(NUM);
    send_queues.resize(NUM);
    for(int i=0; i<NUM; i++){
        std::thread t(&Transport::recv_func, this, i);
        recv_threads.push_back(std::move(t));
    }

    _main_thread = std::move(std::thread(&Transport::main_func, this));
    return 0;
}

void Transport::recv_func(Transport* xport, int index){
    Fdevents *fdes = new Fdevents();
    SelectableQueue<Client*> *accept_queue = &xport->accept_queues[index];
    SelectableQueue<Response> *send_queue = &xport->send_queues[index];
    std::unordered_map<int, Client*> clients;

    fdes->set(accept_queue->fd(), FDEVENT_IN, 0, accept_queue);
    fdes->set(send_queue->fd(), FDEVENT_IN, 0, send_queue);
    
    std::vector<Client*> close_list;
    const Fdevents::events_t* events;

    while (!xport->_close_flag) {
        events = fdes->wait(100);
        if (events == NULL) {
            exit(-1);
        }
        if (events->empty()) {
            continue;
        }

        for (int i = 0; i < (int)events->size(); i++) {
            const Fdevent* fde = events->at(i);
            if (fde->data.ptr == accept_queue) {
                Client* client = NULL;
                accept_queue->pop(&client);
                printf("process %s:%d\n", client->link->remote_ip, client->link->remote_port);

                fdes->set(client->link->fd(), FDEVENT_IN, 0, client);
                clients[client->id] = client;
            } else if (fde->data.ptr == send_queue) {
                while (send_queue->size() > 0) {
                    Response msg;
                    send_queue->pop(&msg);

                    auto it = clients.find(msg.ClientId());
                    if (it == clients.end()) {
                        printf("client %d not found\n", msg.ClientId());
                        continue;
                    }
                    Client* client = it->second;

                    client->link->send(msg);
                    fdes->set(client->link->fd(), FDEVENT_OUT, 0, client);
                }
            } else {
                Client* client = (Client*)fde->data.ptr;
                if (fde->events & FDEVENT_IN) {
                    int ret = client->link->read();
                    if (ret <= 0) {
                        close_list.push_back(client);
                        continue;
                    }
                    while (1) {
                        Message req(client->id);
                        int ret = client->link->recv(&req);
                        if (ret == -1) {
                            close_list.push_back(client);
                            break;
                        } else if (ret == 0) {
                            // not ready
                            break;
                        }
                        xport->_recv_channel->push(req);
                    }
                } else if (fde->events & FDEVENT_OUT) {
                    int ret = client->link->write();
                    if (ret == 0) {
                        fdes->clr(client->link->fd(), FDEVENT_OUT);
                    }
                }
            }
        }

        if (!close_list.empty()) {
            for (auto client : close_list) {
                {
                    std::lock_guard<std::mutex> lk(xport->_mutex);
                    xport->_ids.erase(client->id);
                }
                clients.erase(client->id);

                printf("close %s:%d\n", client->link->remote_ip, client->link->remote_port);
                fdes->del(client->link->fd());
                delete client->link;
                delete client;
            }
            close_list.clear();
        }
    }

    for (auto it : clients) {
        Client* client = it.second;
        delete client->link;
        delete client;
    }
    delete fdes;
}

void Transport::main_func(Transport* xport) {
    Fdevents *fdes = new Fdevents();
    fdes->set(xport->_serv_link->fd(), FDEVENT_IN, 0, xport->_serv_link);
    const Fdevents::events_t* events;

    while (!xport->_close_flag) {
        events = fdes->wait(100);
        if (events == NULL) {
            exit(-1);
        }
        if (events->empty()) {
            continue;
        }

        for (int i = 0; i < (int)events->size(); i++) {
            const Fdevent* fde = events->at(i);
            if (fde->data.ptr == xport->_serv_link) {
                Link* link = xport->_serv_link->accept();
                if (!link) {
                    fprintf(stderr, "%d accept error\n", __LINE__);
                    continue;
                }
                link->noblock(true);
                printf("accept %s:%d\n", link->remote_ip, link->remote_port);

                Client* client = new Client();
                client->link = link;

                while (1) {
                    xport->_id_incr = std::max((int)1, xport->_id_incr + 1);

                    std::lock_guard<std::mutex> lk(xport->_mutex);
                    if (xport->_ids.count(xport->_id_incr) == 0) {
                        client->id = xport->_id_incr;
                        xport->_ids[client->id] = 0;
                        break;
                    }
                }

                int index = client->id % xport->accept_queues.size();
                SelectableQueue<Client*> *queue = &xport->accept_queues[index];
                queue->push(client);
            }
        }
    }

    delete fdes;
}

Message Transport::Recv() {
    return _recv_channel->pop();
}

void Transport::Send(const Response& msg) {
    int index = msg.ClientId() % send_queues.size();
    SelectableQueue<Response> *queue = &send_queues[index];
    queue->push(msg);
}

}; // namespace redis
