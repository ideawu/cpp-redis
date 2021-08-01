#include <vector>
#include <string>

#ifndef NET_MESSAGE_
#define NET_MESSAGE_

namespace redis {

class Message {
public:
    Message() {
        _client_id = -1;
    }
    Message(int client_id) {
        _client_id = client_id;
    }
    Message(const std::vector<std::string>& vals) {
        _vals = vals;
    }
    Message(int client_id, const std::vector<std::string>& vals) {
        _client_id = client_id;
        _vals = vals;
    }

    int ClientId() const {
        return _client_id;
    }
    void SetClientId(int client_id) {
        _client_id = client_id;
    }
    std::string Cmd() const {
        if (_vals.size() > 0) {
            return _vals[0];
        }
        return "";
    }
    void SetCmd(const std::string cmd) {
        if (_vals.empty()) {
            _vals.resize(1);
        }
        _vals[0] = cmd;
    }
    std::string Key() const {
        if (_vals.size() > 1) {
            return _vals[1];
        }
        return "";
    }
    std::string Val() const {
        if (_vals.size() > 2) {
            return _vals[2];
        }
        return "";
    }
    std::vector<std::string> Array() const {
        return _vals;
    }
    std::vector<std::string> Args() const {
        return Args(0);
    }
    std::vector<std::string> Args(size_t offset) const {
        std::vector<std::string> ret;
        if (_vals.size() > offset) {
            ret.assign(_vals.begin() + offset + 1, _vals.end());
        }
        return ret;
    }
    std::string Arg(int idx) const {
        idx += 1;
        if ((int)_vals.size() > idx) {
            return _vals[idx];
        }
        return "";
    }

    std::string Encode() const;
    // 返回解析了多少字节
    int Decode(const std::string& buf);
    int Decode(const char* data, int len);

private:
    int _client_id = -1;
    std::vector<std::string> _vals;
};

}; // namespace redis

#endif
