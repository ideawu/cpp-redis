/*
Copyright (c) 2012-2014 The SSDB Authors. All rights reserved.
Use of this source code is governed by a BSD-style license that can be
found in the LICENSE file.
*/
#ifndef UTIL_QUEUE_H
#define UTIL_QUEUE_H
#include <unistd.h>
#include <pthread.h>
#include <queue>

// Selectable queue, multi writers, single reader
template <class T>
class SelectableQueue {
private:
    int fds[2];
    pthread_mutex_t mutex;
    std::queue<T> items;

public:
    SelectableQueue();
    ~SelectableQueue();
    int fd() {
        return fds[0];
    }
    int size();
    // multi writer
    int push(const T item);
    // single reader
    int pop(T* data);
};


template <class T>
SelectableQueue<T>::SelectableQueue() {
    if (pipe(fds) == -1) {
        fprintf(stderr, "create pipe error\n");
        exit(0);
    }
    pthread_mutex_init(&mutex, NULL);
}

template <class T>
SelectableQueue<T>::~SelectableQueue() {
    pthread_mutex_destroy(&mutex);
    close(fds[0]);
    close(fds[1]);
}

template <class T>
int SelectableQueue<T>::push(const T item) {
    if (pthread_mutex_lock(&mutex) != 0) {
        return -1;
    }
    { items.push(item); }
    if (::write(fds[1], "1", 1) == -1) {
        fprintf(stderr, "write fds error\n");
        exit(0);
    }
    pthread_mutex_unlock(&mutex);
    return 1;
}

template <class T>
int SelectableQueue<T>::size() {
    int ret = 0;
    pthread_mutex_lock(&mutex);
    ret = items.size();
    pthread_mutex_unlock(&mutex);
    return ret;
}

template <class T>
int SelectableQueue<T>::pop(T* data) {
    int n, ret = 1;
    char buf[1];

    while (1) {
        n = ::read(fds[0], buf, 1);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                return -1;
            }
        } else if (n == 0) {
            ret = -1;
        } else {
            if (pthread_mutex_lock(&mutex) != 0) {
                return -1;
            }
            {
                if (items.empty()) {
                    fprintf(stderr, "%s %d error!\n", __FILE__, __LINE__);
                    pthread_mutex_unlock(&mutex);
                    return -1;
                }
                *data = items.front();
                items.pop();
            }
            pthread_mutex_unlock(&mutex);
        }
        break;
    }
    return ret;
}

#endif
