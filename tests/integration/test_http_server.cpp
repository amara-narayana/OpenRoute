#include "tests/test_framework.hpp"
#include "core/storage/database.hpp"
#include "core/gateway/gateway_server.hpp"
#include "core/ui/http_server.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <chrono>
#include <string>
#include <thread>

using namespace openroute;
using namespace openroute::ui;
using namespace openroute::gateway;
using namespace openroute::storage;

namespace {

std::string SimpleHttpGet(uint16_t port, const std::string& path) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) return "";

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    addr.sin_port = htons(port);

    if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        closesocket(sock);
        return "";
    }

    std::string req = "GET " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
    send(sock, req.c_str(), static_cast<int>(req.size()), 0);

    char buf[8192];
    std::string resp;
    int bytes = 0;
    while ((bytes = recv(sock, buf, sizeof(buf) - 1, 0)) > 0) {
        buf[bytes] = '\0';
        resp += buf;
    }

    closesocket(sock);
    return resp;
}

std::string SimpleHttpPost(uint16_t port, const std::string& path, const std::string& json_body) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) return "";

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    addr.sin_port = htons(port);

    if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        closesocket(sock);
        return "";
    }

    std::string req = "POST " + path + " HTTP/1.1\r\n"
                      "Host: 127.0.0.1\r\n"
                      "Content-Type: application/json\r\n"
                      "Content-Length: " + std::to_string(json_body.size()) + "\r\n"
                      "Connection: close\r\n\r\n" + json_body;
    send(sock, req.c_str(), static_cast<int>(req.size()), 0);

    char buf[8192];
    std::string resp;
    int bytes = 0;
    while ((bytes = recv(sock, buf, sizeof(buf) - 1, 0)) > 0) {
        buf[bytes] = '\0';
        resp += buf;
    }

    closesocket(sock);
    return resp;
}

} // namespace

TEST(HttpServerTest, ServeHtmlDashboard) {
    Database db;
    ASSERT_TRUE(db.Open(":memory:"));
    ASSERT_TRUE(db.ApplyMigrations());

    GatewayConfig cfg;
    cfg.id = "gw_test_http";
    GatewayServer gw(db, cfg);
    ASSERT_TRUE(gw.Initialize());

    uint16_t test_port = 8791;
    HttpServer server(gw, test_port);
    ASSERT_TRUE(server.Start());
    EXPECT_TRUE(server.IsRunning());

    // Allow server thread to initialize
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::string resp = SimpleHttpGet(test_port, "/");
    EXPECT_NE(resp.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(resp.find("OpenRoute Gateway"), std::string::npos);
    EXPECT_NE(resp.find("text/html"), std::string::npos);

    server.Stop();
    EXPECT_FALSE(server.IsRunning());
}

TEST(HttpServerTest, ApiStatusAndDiagnostics) {
    Database db;
    ASSERT_TRUE(db.Open(":memory:"));
    ASSERT_TRUE(db.ApplyMigrations());

    GatewayConfig cfg;
    cfg.id = "gw_status_test";
    cfg.name = "My Test Gateway";
    GatewayServer gw(db, cfg);
    ASSERT_TRUE(gw.Initialize());

    uint16_t test_port = 8792;
    HttpServer server(gw, test_port);
    ASSERT_TRUE(server.Start());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::string status_resp = SimpleHttpGet(test_port, "/api/status");
    EXPECT_NE(status_resp.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(status_resp.find("\"gateway_id\":\"gw_status_test\""), std::string::npos);
    EXPECT_NE(status_resp.find("\"gateway_name\":\"My Test Gateway\""), std::string::npos);
    EXPECT_NE(status_resp.find("\"nat_type\""), std::string::npos);

    std::string diag_resp = SimpleHttpGet(test_port, "/api/diagnostics");
    EXPECT_NE(diag_resp.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(diag_resp.find("\"cpu_usage_pct\""), std::string::npos);
    EXPECT_NE(diag_resp.find("\"rtt_ms\""), std::string::npos);

    server.Stop();
}

TEST(HttpServerTest, CreateInvitationAndListDevices) {
    Database db;
    ASSERT_TRUE(db.Open(":memory:"));
    ASSERT_TRUE(db.ApplyMigrations());

    GatewayConfig cfg;
    cfg.id = "gw_invite_test";
    GatewayServer gw(db, cfg);
    ASSERT_TRUE(gw.Initialize());

    uint16_t test_port = 8793;
    HttpServer server(gw, test_port);
    ASSERT_TRUE(server.Start());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Create an invitation
    std::string req_json = "{\"pass_type\": \"TEMPORARY_24H\", \"pass_duration_hours\": 24, \"rate_limit_mbps\": 30, \"single_use\": true}";
    std::string create_resp = SimpleHttpPost(test_port, "/api/invitations/create", req_json);
    EXPECT_NE(create_resp.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(create_resp.find("\"token\""), std::string::npos);
    EXPECT_NE(create_resp.find("\"invitation_url\""), std::string::npos);
    EXPECT_NE(create_resp.find("openroute://invite"), std::string::npos);

    // List devices (empty initially)
    std::string dev_resp = SimpleHttpGet(test_port, "/api/devices");
    EXPECT_NE(dev_resp.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(dev_resp.find("[]"), std::string::npos);

    server.Stop();
}

TEST(HttpServerTest, RemoteInvitationAndNetworkSettings) {
    Database db;
    ASSERT_TRUE(db.Open(":memory:"));
    ASSERT_TRUE(db.ApplyMigrations());

    GatewayConfig cfg;
    cfg.id = "gw_remote_test";
    cfg.name = "Remote Family Gateway";
    GatewayServer gw(db, cfg);
    ASSERT_TRUE(gw.Initialize());

    uint16_t test_port = 8794;
    HttpServer server(gw, test_port);
    ASSERT_TRUE(server.Start());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Configure remote endpoint (DDNS hostname and Cloudflare tunnel URL)
    std::string cfg_json = "{\"public_host\": \"remote.myfamily.org\", \"public_url\": \"https://remote-gw.trycloudflare.com\"}";
    std::string cfg_resp = SimpleHttpPost(test_port, "/api/settings/network", cfg_json);
    EXPECT_NE(cfg_resp.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(cfg_resp.find("\"success\":true"), std::string::npos);
    EXPECT_NE(cfg_resp.find("\"public_host\":\"remote.myfamily.org\""), std::string::npos);

    // Create remote invitation
    std::string invite_req = "{\"network_scope\": \"remote\", \"pass_type\": \"TEMPORARY_24H\", \"pass_duration_hours\": 24}";
    std::string invite_resp = SimpleHttpPost(test_port, "/api/invitations/create", invite_req);
    EXPECT_NE(invite_resp.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(invite_resp.find("https://remote-gw.trycloudflare.com/join"), std::string::npos);
    EXPECT_NE(invite_resp.find("host=remote.myfamily.org"), std::string::npos);
    EXPECT_NE(invite_resp.find("openroute://invite"), std::string::npos);

    // Verify /join endpoint serves landing page
    std::string join_page = SimpleHttpGet(test_port, "/join?token=test_tok&host=remote.myfamily.org");
    EXPECT_NE(join_page.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(join_page.find("OpenRoute Gateway"), std::string::npos);
    EXPECT_NE(join_page.find("Download OpenRoute App"), std::string::npos);

    server.Stop();
}
