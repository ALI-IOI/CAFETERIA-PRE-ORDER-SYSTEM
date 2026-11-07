// WebAssembly demo only: a stand-in for cpp-httplib with just the parts that
// src/server/routes.cpp uses. The browser build (src/wasm/demo_main.cpp) has no
// sockets: the page hands each request to the C++ code, which runs the same
// registerRoutes() handlers as the real server. The native build never sees this
// file; it uses third_party/httplib.
#pragma once
#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace httplib {

struct CaseLess {
    bool operator()(const std::string& a, const std::string& b) const {
        size_t n = a.size() < b.size() ? a.size() : b.size();
        for (size_t i = 0; i < n; i++) {
            char x = a[i] >= 'A' && a[i] <= 'Z' ? (char)(a[i] + 32) : a[i];
            char y = b[i] >= 'A' && b[i] <= 'Z' ? (char)(b[i] + 32) : b[i];
            if (x != y) return x < y;
        }
        return a.size() < b.size();
    }
};
using Headers = std::multimap<std::string, std::string, CaseLess>;
using Params = std::multimap<std::string, std::string>;

struct FormFile {
    std::string name, content, filename, content_type;
};

struct MultipartFormData {
    std::multimap<std::string, std::string> fields;
    std::multimap<std::string, FormFile> files;
    bool has_file(const std::string& key) const { return files.find(key) != files.end(); }
    FormFile get_file(const std::string& key) const {
        auto it = files.find(key);
        return it == files.end() ? FormFile() : it->second;
    }
    std::string get_field(const std::string& key) const {
        auto it = fields.find(key);
        return it == fields.end() ? std::string() : it->second;
    }
};

struct Request {
    std::string method, path, body, remote_addr;
    Headers headers;
    Params params;
    MultipartFormData form;

    bool has_header(const std::string& key) const { return headers.find(key) != headers.end(); }
    std::string get_header_value(const std::string& key, const char* def = "") const {
        auto it = headers.find(key);
        return it == headers.end() ? std::string(def) : it->second;
    }
    bool has_param(const std::string& key) const { return params.find(key) != params.end(); }
    std::string get_param_value(const std::string& key) const {
        auto it = params.find(key);
        return it == params.end() ? std::string() : it->second;
    }
    bool is_multipart_form_data() const {
        return get_header_value("Content-Type").rfind("multipart/form-data", 0) == 0;
    }
};

struct Response {
    int status = -1;
    std::string body;
    Headers headers;
    void set_header(const std::string& key, const std::string& value) { headers.emplace(key, value); }
    void set_content(const std::string& s, const std::string& contentType) {
        body = s;
        headers.erase("Content-Type");
        headers.emplace("Content-Type", contentType);
    }
    void set_redirect(const std::string& url) {
        status = 302;
        set_header("Location", url);
    }
};

class Server {
  public:
    enum class HandlerResponse { Handled, Unhandled };
    using Handler = std::function<void(const Request&, Response&)>;
    using PreHandler = std::function<HandlerResponse(const Request&, Response&)>;

    Server& Get(const std::string& pattern, Handler h) {
        routes_.push_back({"GET", pattern, std::move(h)});
        return *this;
    }
    Server& Post(const std::string& pattern, Handler h) {
        routes_.push_back({"POST", pattern, std::move(h)});
        return *this;
    }
    void set_pre_routing_handler(PreHandler h) { pre_ = std::move(h); }
    void set_post_routing_handler(Handler h) { post_ = std::move(h); }
    void set_payload_max_length(size_t n) { maxPayload_ = n; }
    bool set_mount_point(const std::string&, const std::string&) { return true; }  // pages are static files

    // Exact-path routing, as routes.cpp registers it (no path parameters).
    void dispatch(const Request& req, Response& res) const {
        if (req.body.size() > maxPayload_) {
            res.status = 413;
        } else if (!(pre_ && pre_(req, res) == HandlerResponse::Handled)) {
            bool found = false;
            for (const Route& r : routes_) {
                if (r.method == req.method && r.pattern == req.path) {
                    r.handler(req, res);
                    found = true;
                    break;
                }
            }
            if (!found) res.status = 404;
        }
        if (res.status == -1) res.status = 200;
        if (post_) post_(req, res);
    }

  private:
    struct Route {
        std::string method, pattern;
        Handler handler;
    };
    std::vector<Route> routes_;
    PreHandler pre_;
    Handler post_;
    size_t maxPayload_ = 8 * 1024 * 1024;
};

}  // namespace httplib
