#include <vector>
#include <string>

#ifndef REDIS_RESPONSE_H_
#define REDIS_RESPONSE_H_

namespace redis {

class Response {
public:
    enum { STATUS = 0, INT, NOT_FOUND, BULK, ARRAY };

    Response() {
    }
    Response(int clientId) {
        _clientId = clientId;
    }

    int ClientId() const {
        return _clientId;
    }

    void ReplyOK() {
        _type = STATUS;
    }
    void ReplyError(const std::string msg) {
        _type = STATUS;
        _vals.push_back(msg);
    }
    void ReplyInt(int64_t num) {
        _type = INT;
        _vals.push_back(std::to_string(num));
    }
    void ReplyNotFound() {
        _type = NOT_FOUND;
    }
    void ReplyBulk(const std::string data) {
        _type = BULK;
        _vals.push_back(data);
    }
    void ReplyArray(const std::vector<std::string>& vals) {
        _type = ARRAY;
        this->_vals = vals;
    }
    void ReplyArray(const std::vector<bool>& exists, const std::vector<std::string>& vals) {
        _type = ARRAY;
        _exists = exists;
        _vals = vals;
    }

    std::string Encode() const;

private:
    int _clientId;
    int _type = STATUS;
    std::vector<bool> _exists;
    std::vector<std::string> _vals;
};

}; // namespace redis

#endif
