#pragma once
#include "Arduino.h"
#include <map>
#include <deque>
enum HTTPMethod { HTTP_GET, HTTP_POST };
enum HTTPUploadStatus { UPLOAD_FILE_START, UPLOAD_FILE_WRITE, UPLOAD_FILE_END, UPLOAD_FILE_ABORTED };
#define HTTP_UPLOAD_BUFLEN 1436
struct HTTPUpload { HTTPUploadStatus status; String filename; size_t totalSize; size_t currentSize; uint8_t buf[HTTP_UPLOAD_BUFLEN]; };
struct SimRequest { uint32_t at; std::string uri; std::map<std::string, std::string> args; std::vector<uint8_t> body; bool isUpload = false; bool abortHalfway = false; std::string label; };
struct SimResponse { std::string label; int code; std::string body; uint32_t at; };
class WebServer
{
public:
    typedef std::function<void(void)> Fn;
    struct Route { Fn fn, ufn; };
    std::map<std::string, Route> routes; Fn notFound;
    std::deque<SimRequest> queue; std::vector<SimResponse> responses;
    SimRequest *cur = nullptr; HTTPUpload up; int calls = 0;
    WebServer(int) {}
    void on(const char *u, HTTPMethod, Fn f) { routes[u] = Route{f, nullptr}; }
    void on(const char *u, HTTPMethod, Fn f, Fn uf) { routes[u] = Route{f, uf}; }
    void onNotFound(Fn f) { notFound = f; }
    void enableDelay(boolean) {}
    void begin() {}
    String arg(const String &n) const { if (!cur) return String(""); auto it = cur->args.find(n.s); return it == cur->args.end() ? String("") : String(it->second); }
    bool hasArg(const String &n) const { return cur && cur->args.count(n.s); }
    HTTPUpload &upload() { return up; }
    void sendHeader(const String &, const String &) {}
    void send(int code, const char *, const String &content = String("")) { responses.push_back({cur ? cur->label : "?", code, content.s, g_ms}); }
    void send_P(int code, PGM_P, PGM_P content) { responses.push_back({cur ? cur->label : "?", code, std::string(content).substr(0, 15), g_ms}); }
    void handleClient()
    {
        calls++;
        if (queue.empty() || queue.front().at > g_ms) return;
        SimRequest r = queue.front(); queue.pop_front(); cur = &r;
        auto it = routes.find(r.uri);
        if (it == routes.end()) { if (notFound) notFound(); cur = nullptr; return; }
        if (r.isUpload && it->second.ufn)
        {
            up.status = UPLOAD_FILE_START; up.totalSize = 0; up.currentSize = 0; up.filename = String("x.mjpeg"); it->second.ufn();
            size_t n = r.body.size(), sent = 0; bool aborted = false;
            while (sent < n)
            {
                size_t c = std::min((size_t)HTTP_UPLOAD_BUFLEN, n - sent);
                memcpy(up.buf, r.body.data() + sent, c); up.currentSize = c; up.totalSize += c; up.status = UPLOAD_FILE_WRITE; it->second.ufn();
                sent += c; g_ms += 6; // the phone is not instant: ~240 KB/s
                if (r.abortHalfway && sent > n / 2) { aborted = true; break; }
            }
            if (aborted) { up.status = UPLOAD_FILE_ABORTED; it->second.ufn(); cur = nullptr; return; } // real server sends nothing on abort
            up.currentSize = 0; up.status = UPLOAD_FILE_END; it->second.ufn();
        }
        it->second.fn();
        cur = nullptr;
    }
};
