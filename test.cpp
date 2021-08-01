#include "Transport.h"
#include <sys/time.h>

double microtime() {
    struct timeval now;
    gettimeofday(&now, NULL);
    double ret = now.tv_sec + now.tv_usec / 1000.0 / 1000.0;
    return ret;
}

int main(int argc, char** argv) {
    // std::string buf = "  *2\r\n$1\na\n$2\r\nbc\r\n ";
    // redis::Message msg;
    // int n = msg.Decode(buf);
    // printf("%d\n%s\n", n, msg.Encode().c_str());

    redis::Transport xport;
    xport.Start("127.0.0.1", 6379);
    double stime = microtime();
    int count = 0;
    while (1) {
        redis::Message msg = xport.Recv();
        // printf("req from %d\n", msg.ClientId());
        redis::Response resp(msg.ClientId());
        xport.Send(resp);
        count ++;
        if(count % 100000 == 0){
            double etime = microtime();
            double ts = etime - stime;
            if(ts == 0){
                ts = 1;
            }
            double qps = 100000.0 / ts;
            printf("qps: %.0f\n", qps);
            stime = etime;
        }
    }
    getchar();
    return 0;
}

