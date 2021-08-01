#include "Message.h"

namespace redis {

std::string Message::Encode() const {
    std::string buf;
    buf.push_back('*');
    buf.append(std::to_string(_vals.size()));
    buf.append("\r\n");
    for (auto& p : _vals) {
        buf.push_back('$');
        buf.append(std::to_string(p.size()));
        buf.append("\r\n");
        buf.append(p);
        buf.append("\r\n");
    }
    return buf;
}

int Message::Decode(const std::string& buf) {
    return Decode(buf.data(), buf.size());
}

static int parse_plain(const char* data, int len, std::vector<std::string>* ret);
static int parse(const char* data, int len, std::vector<std::string>* ret);

int Message::Decode(const char* data, int len) {
    return parse(data, len, &_vals);
}

static int parse(const char* data, int len, std::vector<std::string>* ret) {
    ret->clear();

    const int BULK = 0;
    const int ARRAY = 1;
    const int PLAIN = 2;

    const int INIT = 0;
    const int MARK = 1;
    const int SIZE = 2;
    const int DATA = 3;

    int type = 0;
    int bulk = 0;
    int status = INIT;

    int size = 0;
    int s = 0;
    int e = 0;
    while (1) {
        if (e == len) {
            return 0;
        }

        if (status == INIT) {
            char c = data[e];
            if (c == '*') {
                bulk = 0;
                type = ARRAY;
                status = SIZE;
            } else if (c == '$') {
                bulk = 1;
                type = BULK;
                status = SIZE;
            } else if (isalpha(c)) {
                type = PLAIN;
                status = DATA;
            } else if (isspace(c)) {
                s++;
            } else {
                // error
                return -1;
            }
            e++;
            continue;
        }

        if (type == PLAIN) {
            int n = parse_plain(data + s, len - s, ret);
            if (n == 0) {
                return 0;
            }
            return s + n;
        } else {
            if (status == MARK) {
                int c = data[e];
                if (c != '$') {
                    printf("%d error\n", __LINE__);
                    return -1;
                }
                e++;
                size = 0;
                status = SIZE;
            } else if (status == SIZE) {
                int c = data[e];
                if (c == '\r') {
                    if (e > s && data[e - 1] == '\r') {
                        printf("%d error\n", __LINE__);
                        return -1;
                    }
                } else if (c == '\n') {
                    if (type == ARRAY) {
                        bulk = size;
                        type = BULK;
                        status = MARK;
                    } else {
                        status = DATA;
                    }
                    s = e + 1;
                } else if (isdigit(c)) {
                    if (e > s && data[e - 1] == '\r') {
                        printf("%d error\n", __LINE__);
                        return -1;
                    }
                    size = size * 10 + (c - '0');
                } else {
                    printf("%d char: %d, error\n", __LINE__, c);
                    return -1;
                }
                e++;
            } else {
                int end = s + size;
                if (end >= len) {
                    e = (int)len;
                } else {
                    char c = data[end];
                    if (c == '\n') {
                        //
                    } else if (end < (int)len - 1 && c == '\r' && data[end + 1] == '\n') {
                        end += 1;
                    } else {
                        e = len;
                        if (e - s > size + 2) { // \r\n = 2
                            printf("%d error\n", __LINE__);
                            return -1;
                        }
                        continue;
                    }

                    std::string p(data + s, size);
                    ret->push_back(p);

                    status = MARK;
                    bulk--;
                    s = end + 1;
                    e = s;

                    if (bulk == 0) {
                        return e;
                    }
                }
            }
        }
    }
    return 0;
}

static int parse_plain(const char* data, int len, std::vector<std::string>* ret) {
    int s = 0;
    for (int e = 0; e < len; e++) {
        char c = data[e];
        if (c == ' ') {
            int size = e - s;
            std::string p(data + s, size);
            ret->push_back(p);
            s = e + 1;
        } else if (c == '\n') {
            int size = e - s;
            if (size > 0 && data[e - 1] == '\r') {
                size -= 1;
            }
            std::string p(data + s, size);
            ret->push_back(p);
            s = e + 1;
            return s;
        }
    }
    return 0;
}

}; // namespace redis
