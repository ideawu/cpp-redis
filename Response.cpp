#include "Response.h"

namespace redis {

std::string Response::Encode() const {
    std::string buf;
    if (_type == STATUS) {
        if (_vals.empty()) {
            buf.append("+OK\r\n");
        } else {
            buf.append("-ERR ");
            buf.append(_vals[0]);
            buf.append("\r\n");
        }
    } else if (_type == INT) {
        buf.append(":");
        buf.append(_vals[0]);
        buf.append("\r\n");
    } else if (_type == NOT_FOUND) {
        buf.append("$-1\r\n");
    } else if (_type == BULK) {
        const std::string& bulk = _vals[0];
        buf.push_back('$');
        buf.append(std::to_string(bulk.size()));
        buf.append("\r\n");
        buf.append(bulk);
        buf.append("\r\n");
    } else if (_type == ARRAY) {
        buf.push_back('*');
        buf.append(std::to_string(_vals.size()));
        buf.append("\r\n");
        for (int i = 0; i < (int)_vals.size(); i++) {
            auto& p = _vals[i];
            if (i < (int)_exists.size() && _exists[i] == false) {
                buf.append("$-1\r\n");
            } else {
                buf.push_back('$');
                buf.append(std::to_string(p.size()));
                buf.append("\r\n");
                buf.append(p);
                buf.append("\r\n");
            }
        }
    }
    return buf;
}

}; // namespace redis
