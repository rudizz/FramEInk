#pragma once
#include "Arduino.h"
#include <functional>
#include <map>
#include <vector>

enum HTTPMethod { HTTP_GET, HTTP_POST };
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
    void collectHeaders(const char **, size_t) {}
    String header(const char *key) { return requestHeaders[key]; }
    size_t contentLength = 0;
    String arg(const char *key) { return arguments[key]; }
    void setContentLength(size_t size) { contentLength = size; }
    Client client() { return {this}; }
    void call(const char *path, HTTPMethod method = HTTP_GET) {
        routes.at({path, method}).result();
    }
    void on(const char *path, HTTPMethod method, std::function<void()> result,
            std::function<void()> upload = {}) { routes[{path, method}] = {result, upload}; }
    HTTPUpload &upload() { return currentUpload; }
    void sendHeader(const char *, const char *) {}
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
