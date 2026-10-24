#pragma once
#include "Arduino.h"
#include <functional>
#include <map>
#include <vector>

enum HTTPMethod { HTTP_GET, HTTP_POST, HTTP_HEAD };
enum HTTPUploadStatus { UPLOAD_FILE_START, UPLOAD_FILE_WRITE, UPLOAD_FILE_END, UPLOAD_FILE_ABORTED };
struct HTTPUpload {
    HTTPUploadStatus status = UPLOAD_FILE_START;
    String type = "image/jpeg";
    String filename = "../../never-use-client-path.jpg";
    const uint8_t *buf = nullptr;
    size_t currentSize = 0, totalSize = 0;
};

class WebServer {
  public:
    inline static WebServer *lastCreated = nullptr;
    explicit WebServer(int = 80) { lastCreated = this; }
    bool active = false;
    std::function<void()> onHandleClient;
    void begin() { active = true; }
    void stop() { active = false; }
    void handleClient() { if (onHandleClient) onHandleClient(); }
    struct Client {
        WebServer *server;
        bool connected() const { return true; }
        size_t write(const uint8_t *data, size_t size) {
            server->body += String(std::string(reinterpret_cast<const char *>(data), size));
            return size;
        }
        void stop() {}
    };
    struct Route { std::function<void()> result, upload; };
    std::map<std::pair<std::string, HTTPMethod>, Route> routes;
    HTTPUpload currentUpload;
    int status = 0;
    String body;
    std::map<std::string, String> arguments;
    std::map<std::string, String> requestHeaders;
    std::map<std::string, String> responseHeaders;
    std::function<void()> notFound;
    String currentUri;
    HTTPMethod currentMethod = HTTP_GET;
    void collectHeaders(const char **, size_t) {}
    String header(const char *key) { return requestHeaders[key]; }
    size_t contentLength = 0;
    String arg(const char *key) { return arguments[key]; }
    bool hasArg(const char *key) { return arguments.count(key) != 0; }
    void setContentLength(size_t size) { contentLength = size; }
    Client client() { return {this}; }
    String uri() { return currentUri; }
    HTTPMethod method() { return currentMethod; }
    void call(const char *path, HTTPMethod method = HTTP_GET) {
        currentUri = path;
        currentMethod = method;
        status = 0;
        body = "";
        responseHeaders.clear();
        const auto found = routes.find({path, method});
        if (found != routes.end()) found->second.result();
        else if (notFound) notFound();
        else send(404);
    }
    void on(const char *path, HTTPMethod method, std::function<void()> result,
            std::function<void()> upload = {}) { routes[{path, method}] = {result, upload}; }
    HTTPUpload &upload() { return currentUpload; }
    void onNotFound(std::function<void()> handler) { notFound = handler; }
    void sendHeader(const String &key, const String &value, bool = false) { responseHeaders[key.c_str()] = value; }
    void send(int code, const char * = "", String text = "") { status = code; body = text; }
    void send_P(int code, const char *, const char *, size_t) { status = code; }
    void event(HTTPUploadStatus event, const std::vector<uint8_t> &bytes = {}, size_t total = 0) {
        currentUpload.status = event;
        currentUpload.buf = bytes.data();
        currentUpload.currentSize = bytes.size();
        currentUpload.totalSize = total;
        routes.at({"/api/photos", HTTP_POST}).upload();
    }
    void result() { routes.at({"/api/photos", HTTP_POST}).result(); }
};
