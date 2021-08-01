#ifndef NET_TRANSPORT_
#define NET_TRANSPORT_
#include <unordered_map>
#include <thread>
#include <mutex>
#include <atomic>
#include "Message.h"
#include "Response.h"
#include "SelectableQueue.h"

template <class T>
class Channel;
class Fdevents;

namespace redis {

class Link;

class Transport {
public:
    Transport();
    ~Transport();

    int Start(const std::string& ip, int port);

    // TODO: 优化
    Message Recv();
    void Send(const Response& resp);

private:
    struct Client {
        int id;
        Link* link;
    };

    static void main_func(Transport* xport);
    std::thread _main_thread;

    static void recv_func(Transport* xport, int index);
    std::vector<std::thread> recv_threads;
    std::vector<SelectableQueue<Client*>> accept_queues;
    std::vector<SelectableQueue<Response>> send_queues;

    int _id_incr;
    Link* _serv_link;
    Channel<Message>* _recv_channel;
    std::atomic<bool> _close_flag;

    std::mutex _mutex;
    std::unordered_map<int, int> _ids;
};

}; // namespace redis

#endif
