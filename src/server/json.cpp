#include "server/json.h"
#include <cstdio>

std::string jsonEscape(const std::string& s) {
    std::string out;
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '<': out += "\\u003c"; break;  // never lets "</script>" appear in output
            case '>': out += "\\u003e"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out += (char)c;
                }
        }
    }
    return out;
}

void Json::comma() {
    if (afterKey_) {
        afterKey_ = false;
        return;
    }
    if (!first_[depth_]) s_ += ",";
    first_[depth_] = false;
}

Json& Json::beginObject() {
    comma();
    s_ += "{";
    first_[++depth_] = true;
    return *this;
}

Json& Json::endObject() {
    s_ += "}";
    depth_--;
    return *this;
}

Json& Json::beginArray() {
    comma();
    s_ += "[";
    first_[++depth_] = true;
    return *this;
}

Json& Json::endArray() {
    s_ += "]";
    depth_--;
    return *this;
}

Json& Json::key(const char* k) {
    comma();
    s_ += "\"";
    s_ += jsonEscape(k);
    s_ += "\":";
    afterKey_ = true;
    return *this;
}

Json& Json::value(const std::string& v) {
    comma();
    s_ += "\"" + jsonEscape(v) + "\"";
    return *this;
}

Json& Json::value(const char* v) { return value(std::string(v)); }

Json& Json::value(long long v) {
    comma();
    s_ += std::to_string(v);
    return *this;
}

Json& Json::value(bool v) {
    comma();
    s_ += v ? "true" : "false";
    return *this;
}

std::string jsonError(const std::string& message) {
    return "{\"ok\":false,\"error\":\"" + jsonEscape(message) + "\"}";
}
