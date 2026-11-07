/* AudioScope by BerryStudio 0.8-alpha — local live bridge.
 *
 * A deliberately tiny HTTP/1.1 + WebSocket server bound to 127.0.0.1. It
 * serves the copied Web UI and pushes the same snapshot JSON at a fixed
 * interval, so the browser needs no Python server and no network access
 * beyond loopback. No third-party dependency is used.
 */
#ifndef AS_APP_LIVE_SERVER_HPP
#define AS_APP_LIVE_SERVER_HPP

#include <string>

namespace audioscope {

struct LiveOptions {
    std::string root;            /* directory with index.html */
    int port = 8124;             /* 0 = pick a free loopback port */
    int interval_ms = 500;       /* snapshot push interval */
    bool capture = false;        /* opt-in: capture the default/selected input */
    int device = -1;             /* input device index for --capture */
};

class LiveServer {
public:
    explicit LiveServer(LiveOptions options);
    ~LiveServer();
    LiveServer(const LiveServer &) = delete;
    LiveServer &operator=(const LiveServer &) = delete;

    bool start(std::string *error);
    void stop();
    int port() const;

private:
    struct Impl;
    Impl *impl_;
};

/* Connect to the server, perform the WebSocket handshake, read one snapshot
 * frame and a static index.html response. Used by `ui --live --smoke` so
 * `make test` covers the transport without a browser. */
int live_bridge_smoke(int port, std::string *error);

} // namespace audioscope

#endif
