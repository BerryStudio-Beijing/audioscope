/* AudioScope by BerryStudio 0.8-alpha — local live bridge. */
#include "app/live_server.hpp"

#include "app/ui_snapshot.hpp"
#include "core/api.h"
#include "core/fft.h"
#include "core/window.h"
#include "platform/audio_backend.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <csignal>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace audioscope {
namespace {

#ifdef _WIN32
using as_socket = SOCKET;
using as_socklen = int;
constexpr as_socket kBadSocket = INVALID_SOCKET;
int socket_error() { return WSAGetLastError(); }
void close_socket(as_socket s) { closesocket(s); }
#else
using as_socket = int;
using as_socklen = socklen_t;
constexpr as_socket kBadSocket = -1;
int socket_error() { return errno; }
void close_socket(as_socket s) { ::close(s); }
#endif

constexpr int kFftSize = 4096;
constexpr int kScopeSamples = 720;

void set_socket_timeouts(as_socket s, int recv_ms, int send_ms) {
#ifdef _WIN32
    DWORD tv = (DWORD)recv_ms;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
    tv = (DWORD)send_ms;
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof(tv));
#else
    timeval tv;
    tv.tv_sec = recv_ms / 1000;
    tv.tv_usec = (recv_ms % 1000) * 1000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    tv.tv_sec = send_ms / 1000;
    tv.tv_usec = (send_ms % 1000) * 1000;
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
}

bool send_all(as_socket s, const char *data, size_t len) {
    size_t off = 0;
    while (off < len) {
        int n = (int)send(s, data + off, (int)(len - off), 0);
        if (n <= 0) {
            if (n < 0 && socket_error() ==
#ifndef _WIN32
                             EINTR
#else
                             WSAEINTR
#endif
            )
                continue;
            return false;
        }
        off += (size_t)n;
    }
    return true;
}

std::string to_lower(std::string s) {
    for (char &c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

std::string header_value(const std::string &headers, const char *name) {
    std::string needle = to_lower(name);
    size_t pos = 0;
    while (pos < headers.size()) {
        size_t end = headers.find("\r\n", pos);
        if (end == std::string::npos) end = headers.size();
        std::string line = headers.substr(pos, end - pos);
        size_t colon = line.find(':');
        if (colon != std::string::npos && to_lower(line.substr(0, colon)) == needle) {
            size_t v = line.find_first_not_of(" \t", colon + 1);
            if (v == std::string::npos) return "";
            return line.substr(v);
        }
        pos = end + 2;
    }
    return "";
}

/* --------------------------------------------------------------- SHA-1 */
uint32_t rol(uint32_t v, int bits) { return (v << bits) | (v >> (32 - bits)); }

void sha1_block(uint32_t h[5], const uint8_t *p) {
    uint32_t w[80];
    for (int i = 0; i < 16; ++i)
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
               ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
    for (int i = 16; i < 80; ++i)
        w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 80; ++i) {
        uint32_t f, k;
        if (i < 20) { f = (b & c) | ((~b) & d); k = 0x5A827999u; }
        else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1u; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDCu; }
        else { f = b ^ c ^ d; k = 0xCA62C1D6u; }
        uint32_t t = rol(a, 5) + f + e + k + w[i];
        e = d; d = c; c = rol(b, 30); b = a; a = t;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
}

void sha1(const std::string &data, uint8_t out[20]) {
    uint32_t h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
    size_t n = data.size(), i = 0;
    for (; i + 64 <= n; i += 64) sha1_block(h, (const uint8_t *)data.data() + i);
    uint8_t buf[128];
    size_t rem = n - i;
    if (rem) std::memcpy(buf, data.data() + i, rem);
    buf[rem] = 0x80;
    size_t total = rem + 1;
    while (total % 64 != 56) buf[total++] = 0;
    uint64_t bits = (uint64_t)n * 8u;
    for (int j = 0; j < 8; ++j) buf[total + (size_t)j] = (uint8_t)(bits >> (56 - 8 * j));
    total += 8;
    for (size_t j = 0; j < total; j += 64) sha1_block(h, buf + j);
    for (int j = 0; j < 5; ++j) {
        out[j * 4] = (uint8_t)(h[j] >> 24);
        out[j * 4 + 1] = (uint8_t)(h[j] >> 16);
        out[j * 4 + 2] = (uint8_t)(h[j] >> 8);
        out[j * 4 + 3] = (uint8_t)h[j];
    }
}

std::string base64(const uint8_t *p, size_t n) {
    static const char *tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (size_t i = 0; i < n; i += 3) {
        uint32_t v = (uint32_t)p[i] << 16;
        int m = 1;
        if (i + 1 < n) { v |= (uint32_t)p[i + 1] << 8; ++m; }
        if (i + 2 < n) { v |= (uint32_t)p[i + 2]; ++m; }
        out.push_back(tbl[(v >> 18) & 63]);
        out.push_back(tbl[(v >> 12) & 63]);
        out.push_back(m > 1 ? tbl[(v >> 6) & 63] : '=');
        out.push_back(m > 2 ? tbl[v & 63] : '=');
    }
    return out;
}

std::string ws_accept(const std::string &key) {
    uint8_t digest[20];
    sha1(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11", digest);
    return base64(digest, sizeof(digest));
}

bool ws_send_text(as_socket s, const std::string &text) {
    std::string frame;
    size_t n = text.size();
    frame.push_back((char)0x81);
    if (n < 126) {
        frame.push_back((char)n);
    } else if (n <= 0xFFFF) {
        frame.push_back((char)126);
        frame.push_back((char)((n >> 8) & 0xFF));
        frame.push_back((char)(n & 0xFF));
    } else {
        frame.push_back((char)127);
        for (int i = 7; i >= 0; --i) frame.push_back((char)((n >> (8 * i)) & 0xFF));
    }
    return send_all(s, frame.data(), frame.size()) &&
           send_all(s, text.data(), text.size());
}

/* ----------------------------------------------------------- static files */
std::string mime_type(const std::string &path) {
    size_t dot = path.find_last_of('.');
    std::string ext = dot == std::string::npos ? "" : to_lower(path.substr(dot));
    if (ext == ".html") return "text/html; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".js") return "text/javascript; charset=utf-8";
    if (ext == ".json") return "application/json";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".png") return "image/png";
    if (ext == ".ico") return "image/x-icon";
    if (ext == ".woff2") return "font/woff2";
    if (ext == ".woff") return "font/woff";
    return "application/octet-stream";
}

bool read_file(const std::string &path, std::string *out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    out->assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return true;
}

std::string http_response(int code, const char *status, const std::string &type,
                          const std::string &body) {
    char head[256];
    std::snprintf(head, sizeof(head),
                  "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
                  "Cache-Control: no-store\r\nConnection: close\r\n\r\n",
                  code, status, type.c_str(), body.size());
    return std::string(head) + body;
}

bool read_http_headers(as_socket s, std::string *headers) {
    headers->clear();
    char buf[1024];
    for (;;) {
        int n = (int)recv(s, buf, (int)sizeof(buf), 0);
        if (n <= 0) return false;
        headers->append(buf, (size_t)n);
        if (headers->find("\r\n\r\n") != std::string::npos) return true;
        if (headers->size() > 16384) return false;
    }
}

/* ------------------------------------------------------------ live source */
class LiveSource {
public:
    LiveSource() : fft_ok_(as_fft_init(&fft_, kFftSize) == 0) {
        scope_.assign((size_t)kScopeSamples, 0.0f);
        hist_.assign((size_t)kFftSize, 0.0f);
        win_.resize((size_t)kFftSize);
        spec_re_.resize((size_t)kFftSize);
        mag_.resize((size_t)kFftSize / 2 + 1);
        if (fft_ok_) as_window_hann(win_.data(), kFftSize);
    }
    ~LiveSource() {
        if (audio_) as_audio_close(audio_);
        if (fft_ok_) as_fft_free(&fft_);
    }

    void open(const LiveOptions &opt, std::string *warning) {
        if (!opt.capture) return;
        audio_ = as_audio_open(opt.device, -2, 48000.0, 256);
        if (!audio_) {
            if (warning) *warning = as_audio_last_error();
            return;
        }
        channels_ = std::clamp(as_audio_channels(audio_), 1, 8);
        double fs = as_audio_rate(audio_);
        if (fs > 0.0) fs_ = fs;
        AsDeviceInfo devs[64];
        int n = as_platform_list_devices(devs, 64);
        for (int i = 0; i < n; ++i) {
            if (!devs[i].is_input) continue;
            if (opt.device < 0 || opt.device == i) {
                device_ = devs[i].name;
                break;
            }
        }
        source_ = "live · capture";
    }

    bool capturing() const { return audio_ != nullptr; }
    const std::string &device() const { return device_; }
    const std::string &source() const { return source_; }
    int channels() const { return channels_; }

    void fill(UiSnapshotData *d) {
        if (audio_) capture_frames();
        else synthesize_frames();

        d->fs = fs_;
        d->channels = channels_;
        d->scope = scope_;
        d->source = source_.c_str();
        d->device = device_.c_str();
        d->mode = "Inst";
        d->platform = as_platform_name();
        d->timestamp = 0.0;
        as_stats_compute(scope_.data(), (int)scope_.size(), fs_, &d->stats);
        d->clip = 0;
        for (float v : scope_) {
            if (std::fabs(v) >= 0.999f) { d->clip = 1; break; }
        }

        for (int i = 0; i < kFftSize; ++i) spec_re_[(size_t)i] = (double)hist_[(size_t)i] * win_[(size_t)i];
        if (fft_ok_) {
            as_fft_forward(&fft_, spec_re_.data(), mag_.data(), kFftSize);
            ui_spectrum_from_mag(mag_.data(), kFftSize, fs_, &d->spec_freqs, &d->spec_db);
            as_tone_analyze(hist_.data(), kFftSize, fs_, &d->tone);
        }
    }

private:
    void capture_frames() {
        std::vector<float> buf((size_t)kScopeSamples * (size_t)channels_, 0.0f);
        int got = as_audio_read(audio_, buf.data(), kScopeSamples);
        if (got <= 0) return;
        std::vector<float> mono((size_t)got);
        for (int i = 0; i < got; ++i) mono[(size_t)i] = buf[(size_t)i * (size_t)channels_];
        append_history(mono);
    }

    void synthesize_frames() {
        std::vector<float> mono((size_t)kScopeSamples);
        double freq = 1000.0 + 80.0 * std::sin(phase_ * 0.05);
        for (int i = 0; i < kScopeSamples; ++i) {
            double t = phase_ + 2.0 * 3.14159265358979323846 * freq * (double)i / fs_;
            mono[(size_t)i] = (float)(0.5 * std::sin(t) + 0.0008 * std::sin(t * 17.0));
        }
        phase_ += 2.0 * 3.14159265358979323846 * freq * (double)kScopeSamples / fs_;
        append_history(mono);
    }

    void append_history(const std::vector<float> &mono) {
        size_t n = std::min(mono.size(), (size_t)kScopeSamples);
        if (n < scope_.size()) {
            std::memmove(scope_.data(), scope_.data() + n,
                         (scope_.size() - n) * sizeof(float));
        }
        std::memcpy(scope_.data() + (scope_.size() - n), mono.data() + (mono.size() - n),
                    n * sizeof(float));

        size_t m = std::min(mono.size(), hist_.size());
        std::memmove(hist_.data(), hist_.data() + m, (hist_.size() - m) * sizeof(float));
        std::memcpy(hist_.data() + (hist_.size() - m), mono.data() + (mono.size() - m),
                    m * sizeof(float));
    }

    AsAudio *audio_ = nullptr;
    int channels_ = 2;
    double fs_ = 48000.0;
    std::string device_ = "default input";
    std::string source_ = "live · synthetic";
    double phase_ = 0.0;
    std::vector<float> scope_;
    std::vector<float> hist_;
    std::vector<double> win_;
    std::vector<double> spec_re_;
    std::vector<double> mag_;
    AsFft fft_{};
    bool fft_ok_ = false;
};

} // namespace

struct LiveServer::Impl {
    LiveOptions opt;
    as_socket listen_fd = kBadSocket;
    int bound_port = 0;
    std::atomic<bool> stop{false};
    bool started = false;
    bool wsa = false;
    std::thread thread;
    LiveSource source;

    explicit Impl(LiveOptions o) : opt(std::move(o)) {}

    bool start(std::string *error) {
#ifdef _WIN32
        WSADATA wsa_data;
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
            if (error) *error = "WSAStartup failed";
            return false;
        }
        wsa = true;
#endif
        std::string warning;
        source.open(opt, &warning);
        if (!warning.empty())
            std::fprintf(stderr, "live: capture unavailable (%s); streaming synthetic data.\n",
                         warning.c_str());

        listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd == kBadSocket) {
            if (error) *error = "socket() failed";
            return false;
        }
        int one = 1;
        setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof(one));
        set_socket_timeouts(listen_fd, 3000, 1000);

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons((unsigned short)opt.port);
        if (bind(listen_fd, (sockaddr *)&addr, sizeof(addr)) != 0) {
            if (error) *error = "bind 127.0.0.1 failed (port in use?)";
            close_socket(listen_fd);
            listen_fd = kBadSocket;
            return false;
        }
        if (listen(listen_fd, 8) != 0) {
            if (error) *error = "listen() failed";
            close_socket(listen_fd);
            listen_fd = kBadSocket;
            return false;
        }
        sockaddr_in bound{};
        as_socklen len = sizeof(bound);
        if (getsockname(listen_fd, (sockaddr *)&bound, &len) == 0)
            bound_port = ntohs(bound.sin_port);
        else
            bound_port = opt.port;

        stop.store(false);
        started = true;
        thread = std::thread([this] { run(); });
        return true;
    }

    void stop_server() {
        if (!started) return;
        started = false;
        stop.store(true);
        if (thread.joinable()) thread.join();
        if (listen_fd != kBadSocket) {
            close_socket(listen_fd);
            listen_fd = kBadSocket;
        }
#ifdef _WIN32
        if (wsa) { WSACleanup(); wsa = false; }
#endif
    }

    void handle_connection(as_socket client, std::vector<as_socket> *clients) {
        std::string headers;
        if (!read_http_headers(client, &headers)) {
            close_socket(client);
            return;
        }
        size_t line_end = headers.find("\r\n");
        std::string line = headers.substr(0, line_end);
        size_t sp1 = line.find(' ');
        size_t sp2 = line.find(' ', sp1 == std::string::npos ? 0 : sp1 + 1);
        std::string path = "/";
        if (sp1 != std::string::npos)
            path = line.substr(sp1 + 1, sp2 == std::string::npos ? std::string::npos : sp2 - sp1 - 1);
        size_t q = path.find('?');
        if (q != std::string::npos) path.resize(q);

        if (path == "/ws") {
            /* Loopback bind is not enough: reject cross-site WebSocket
             * attempts from pages served elsewhere. Non-browser clients
             * (the --smoke probe) may omit Origin. */
            std::string origin = header_value(headers, "Origin");
            if (!origin.empty()) {
                std::string ok1 = "http://127.0.0.1:" + std::to_string(bound_port);
                std::string ok2 = "http://localhost:" + std::to_string(bound_port);
                if (origin != ok1 && origin != ok2) {
                    std::string resp = http_response(403, "Forbidden", "text/plain", "bad origin\n");
                    send_all(client, resp.data(), resp.size());
                    close_socket(client);
                    return;
                }
            }
            std::string key = header_value(headers, "Sec-WebSocket-Key");
            if (key.empty()) {
                std::string resp = http_response(400, "Bad Request", "text/plain", "missing key\n");
                send_all(client, resp.data(), resp.size());
                close_socket(client);
                return;
            }
            std::string resp = "HTTP/1.1 101 Switching Protocols\r\n"
                               "Upgrade: websocket\r\n"
                               "Connection: Upgrade\r\n"
                               "Sec-WebSocket-Accept: " + ws_accept(key) + "\r\n\r\n";
            if (send_all(client, resp.data(), resp.size())) clients->push_back(client);
            else close_socket(client);
            return;
        }

        if (path == "/" || path.empty()) path = "/index.html";
        if (path.find("..") != std::string::npos || path.find('\\') != std::string::npos ||
            path.find(':') != std::string::npos || path[0] != '/') {
            std::string resp = http_response(400, "Bad Request", "text/plain", "bad path\n");
            send_all(client, resp.data(), resp.size());
            close_socket(client);
            return;
        }
        std::string rel = path.substr(1);
        std::string file = (std::filesystem::path(opt.root) / rel).string();
        std::string body;
        if (!read_file(file, &body)) {
            std::string resp = http_response(404, "Not Found", "text/plain", "not found\n");
            send_all(client, resp.data(), resp.size());
        } else {
            std::string resp = http_response(200, "OK", mime_type(rel), body);
            send_all(client, resp.data(), resp.size());
        }
        close_socket(client);
    }

    void broadcast(std::vector<as_socket> *clients, const std::string &json) {
        std::vector<as_socket> alive;
        alive.reserve(clients->size());
        for (as_socket fd : *clients) {
            if (ws_send_text(fd, json)) alive.push_back(fd);
            else close_socket(fd);
        }
        *clients = std::move(alive);
    }

    void run() {
        std::vector<as_socket> clients;
        auto next_push = std::chrono::steady_clock::now() +
                         std::chrono::milliseconds(opt.interval_ms);
        while (!stop.load()) {
            fd_set reads;
            FD_ZERO(&reads);
            FD_SET(listen_fd, &reads);
            timeval tv{0, 50000};
#ifdef _WIN32
            int ready = select(0, &reads, nullptr, nullptr, &tv);
#else
            int ready = select(listen_fd + 1, &reads, nullptr, nullptr, &tv);
#endif
            if (ready > 0 && FD_ISSET(listen_fd, &reads)) {
                as_socket client = accept(listen_fd, nullptr, nullptr);
                if (client != kBadSocket) {
                    set_socket_timeouts(client, 3000, 1000);
                    handle_connection(client, &clients);
                }
            }
            auto now = std::chrono::steady_clock::now();
            if (now >= next_push && !clients.empty()) {
                UiSnapshotData data;
                source.fill(&data);
                broadcast(&clients, ui_snapshot_json(data));
                next_push = now + std::chrono::milliseconds(opt.interval_ms);
            } else if (now >= next_push) {
                next_push = now + std::chrono::milliseconds(opt.interval_ms);
            }
        }
        for (as_socket fd : clients) close_socket(fd);
    }
};

LiveServer::LiveServer(LiveOptions options) : impl_(new Impl(std::move(options))) {}

LiveServer::~LiveServer() {
    stop();
    delete impl_;
}

bool LiveServer::start(std::string *error) {
#ifndef _WIN32
    std::signal(SIGPIPE, SIG_IGN);
#endif
    return impl_->start(error);
}

void LiveServer::stop() {
    if (impl_) impl_->stop_server();
}

int LiveServer::port() const { return impl_ ? impl_->bound_port : 0; }

/* ------------------------------------------------------------------ smoke */
namespace {

as_socket connect_loopback(int port) {
    as_socket s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == kBadSocket) return kBadSocket;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((unsigned short)port);
    if (connect(s, (sockaddr *)&addr, sizeof(addr)) != 0) {
        close_socket(s);
        return kBadSocket;
    }
    set_socket_timeouts(s, 4000, 4000);
    return s;
}

bool recv_headers(as_socket fd, std::string *out) {
    char buf[512];
    for (;;) {
        int n = (int)recv(fd, buf, (int)sizeof(buf), 0);
        if (n <= 0) return false;
        out->append(buf, (size_t)n);
        if (out->find("\r\n\r\n") != std::string::npos) return true;
        if (out->size() > 65536) return false;
    }
}

} // namespace

int live_bridge_smoke(int port, std::string *error) {
    as_socket fd = connect_loopback(port);
    if (fd == kBadSocket) {
        if (error) *error = "connect failed";
        return 1;
    }
    std::string req = "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
    if (!send_all(fd, req.data(), req.size())) {
        if (error) *error = "GET / send failed";
        close_socket(fd);
        return 1;
    }
    std::string resp;
    if (!recv_headers(fd, &resp) || resp.find("200 OK") == std::string::npos ||
        resp.find("<!DOCTYPE html>") == std::string::npos) {
        if (error) *error = "static index.html not served";
        close_socket(fd);
        return 1;
    }
    close_socket(fd);

    fd = connect_loopback(port);
    if (fd == kBadSocket) {
        if (error) *error = "ws connect failed";
        return 1;
    }
    req = "GET /ws HTTP/1.1\r\nHost: 127.0.0.1\r\nUpgrade: websocket\r\n"
          "Connection: Upgrade\r\nSec-WebSocket-Version: 13\r\n"
          "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n";
    if (!send_all(fd, req.data(), req.size())) {
        if (error) *error = "ws handshake send failed";
        close_socket(fd);
        return 1;
    }
    resp.clear();
    bool upgraded = false;
    char buf[1024];
    for (;;) {
        int n = (int)recv(fd, buf, (int)sizeof(buf), 0);
        if (n <= 0) break;
        resp.append(buf, (size_t)n);
        size_t end = resp.find("\r\n\r\n");
        if (end != std::string::npos) {
            upgraded = resp.find("101 Switching Protocols") != std::string::npos &&
                       resp.find("s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") != std::string::npos;
            resp.erase(0, end + 4);
            break;
        }
    }
    if (!upgraded) {
        if (error) *error = "websocket handshake rejected";
        close_socket(fd);
        return 1;
    }

    /* Wait for the next frame; it may already be in resp. */
    std::string payload;
    auto read_more = [&]() {
        int n = (int)recv(fd, buf, (int)sizeof(buf), 0);
        if (n > 0) resp.append(buf, (size_t)n);
        return n;
    };
    while (resp.size() < 2) {
        if (read_more() <= 0) break;
    }
    if (resp.size() >= 2) {
        unsigned char b1 = (unsigned char)resp[1];
        uint64_t len = b1 & 0x7F;
        size_t pos = 2;
        if (len == 126) {
            while (resp.size() < 4) if (read_more() <= 0) break;
            len = ((uint64_t)(unsigned char)resp[2] << 8) | (unsigned char)resp[3];
            pos = 4;
        } else if (len == 127) {
            while (resp.size() < 10) if (read_more() <= 0) break;
            len = 0;
            for (int i = 0; i < 8; ++i) len = (len << 8) | (unsigned char)resp[2 + i];
            pos = 10;
        }
        while (resp.size() < pos + len) if (read_more() <= 0) break;
        if (resp.size() >= pos + len) payload = resp.substr(pos, (size_t)len);
    }
    close_socket(fd);
    if (payload.find("window.AUDIOSCOPE_DATA") == std::string::npos ||
        payload.find("scope:") == std::string::npos ||
        payload.find("meta:") == std::string::npos) {
        if (error) *error = "websocket frame payload did not contain the snapshot";
        return 1;
    }
    return 0;
}

} // namespace audioscope
