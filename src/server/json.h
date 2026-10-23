// Minimal JSON writer for API responses (output only; requests are form-encoded).
#pragma once
#include <string>

std::string jsonEscape(const std::string& s);

// Builds JSON text directly into a string. Nesting depth up to 16.
//   Json j; j.beginObject(); j.key("ok").value(true); j.endObject();
class Json {
  public:
    Json& beginObject();
    Json& endObject();
    Json& beginArray();
    Json& endArray();
    Json& key(const char* k);
    Json& value(const std::string& v);
    Json& value(const char* v);
    Json& value(long long v);
    Json& value(int v) { return value((long long)v); }
    Json& value(bool v);
    const std::string& str() const { return s_; }

  private:
    void comma();
    std::string s_;
    bool first_[16] = {true};
    int depth_ = 0;
    bool afterKey_ = false;
};

// {"ok":false,"error":"..."}
std::string jsonError(const std::string& message);
