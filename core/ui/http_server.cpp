#include "core/ui/http_server.hpp"
#include "core/common/logger.hpp"
#include "apps/windows/dashboard_assets.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <sstream>
#include <vector>

using json = nlohmann::json;

namespace openroute::ui {

HttpServer::HttpServer(gateway::GatewayServer& gateway, uint16_t port)
    : gateway_(gateway), port_(port) {
}

HttpServer::~HttpServer() {
    Stop();
}

bool HttpServer::Start() {
    if (is_running_) return true;

#if defined(_WIN32)
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        LOG_ERROR << "[HttpServer] WSAStartup failed";
        return false;
    }
#endif

    listen_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket_ == INVALID_SOCKET) {
        LOG_ERROR << "[HttpServer] Failed to create socket";
        return false;
    }

    int opt = 1;
    setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);
    server_addr.sin_port = htons(port_);

    if (bind(listen_socket_, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == SOCKET_ERROR) {
        LOG_ERROR << "[HttpServer] Bind failed on port " << port_;
        closesocket(listen_socket_);
        listen_socket_ = INVALID_SOCKET;
        return false;
    }

    if (listen(listen_socket_, 10) == SOCKET_ERROR) {
        LOG_ERROR << "[HttpServer] Listen failed on port " << port_;
        closesocket(listen_socket_);
        listen_socket_ = INVALID_SOCKET;
        return false;
    }

    // Set 500ms timeout on accept so we can exit cleanly
    DWORD timeout_ms = 500;
    setsockopt(listen_socket_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));

    is_running_ = true;
    server_thread_ = std::thread(&HttpServer::ServerLoop, this);
    LOG_INFO << "[HttpServer] Dashboard HTTP server listening at http://127.0.0.1:" << port_;
    return true;
}

void HttpServer::Stop() {
    if (!is_running_) return;

    is_running_ = false;
    if (listen_socket_ != INVALID_SOCKET) {
        closesocket(listen_socket_);
        listen_socket_ = INVALID_SOCKET;
    }

    if (server_thread_.joinable()) {
        server_thread_.join();
    }

#if defined(_WIN32)
    WSACleanup();
#endif

    LOG_INFO << "[HttpServer] Dashboard HTTP server stopped";
}

void HttpServer::ServerLoop() {
    while (is_running_) {
        sockaddr_in client_addr{};
        int client_len = sizeof(client_addr);
        SOCKET client_sock = accept(listen_socket_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);

        if (client_sock == INVALID_SOCKET) {
            continue;
        }

        std::thread([this, client_sock]() {
            HandleClient(client_sock);
        }).detach();
    }
}

void HttpServer::HandleClient(SOCKET client_socket) {
    char buffer[4096];
    int bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0) {
        closesocket(client_socket);
        return;
    }

    std::string req(buffer, bytes_read);
    size_t header_end = req.find("\r\n\r\n");
    while (header_end == std::string::npos) {
        bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_read <= 0) break;
        req.append(buffer, bytes_read);
        header_end = req.find("\r\n\r\n");
    }

    std::istringstream stream(req);
    std::string method, path, protocol;
    stream >> method >> path >> protocol;

    // Check for Content-Length
    size_t expected_content_len = 0;
    size_t cl_pos = req.find("Content-Length:");
    if (cl_pos == std::string::npos) cl_pos = req.find("content-length:");
    if (cl_pos != std::string::npos) {
        size_t line_end = req.find("\r\n", cl_pos);
        if (line_end != std::string::npos) {
            std::string val_str = req.substr(cl_pos + 15, line_end - (cl_pos + 15));
            try {
                expected_content_len = std::stoul(val_str);
            } catch (...) {}
        }
    }

    std::string body;
    if (header_end != std::string::npos) {
        body = req.substr(header_end + 4);
        while (body.size() < expected_content_len) {
            bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
            if (bytes_read <= 0) break;
            body.append(buffer, bytes_read);
        }
    }

    std::string response = HandleRequest(method, path, body);
    send(client_socket, response.c_str(), static_cast<int>(response.size()), 0);
    closesocket(client_socket);
}

std::string HttpServer::HandleRequest(const std::string& method, const std::string& path, const std::string& body) {
    if (method == "OPTIONS") {
        return "HTTP/1.1 204 No Content\r\n"
               "Access-Control-Allow-Origin: *\r\n"
               "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
               "Access-Control-Allow-Headers: Content-Type\r\n"
               "Connection: close\r\n\r\n";
    }

    if (method == "GET" && (path == "/" || path == "/index.html")) {
        std::string html = GetDashboardHtml();
        std::ostringstream ss;
        ss << "HTTP/1.1 200 OK\r\n"
           << "Content-Type: text/html; charset=utf-8\r\n"
           << "Content-Length: " << html.size() << "\r\n"
           << "Connection: close\r\n\r\n"
           << html;
        return ss.str();
    }

    std::string json_response;
    if (method == "GET" && path == "/api/status") {
        json_response = HandleGetStatus();
    } else if (method == "GET" && path == "/api/devices") {
        json_response = HandleGetDevices();
    } else if (method == "POST" && path == "/api/devices/approve") {
        json_response = HandleApproveDevice(body);
    } else if (method == "POST" && path == "/api/devices/reject") {
        json_response = HandleRejectDevice(body);
    } else if (method == "POST" && path == "/api/devices/pause") {
        json_response = HandlePauseDevice(body);
    } else if (method == "POST" && path == "/api/invitations/create") {
        json_response = HandleCreateInvitation(body);
    } else if (method == "GET" && path == "/api/diagnostics") {
        json_response = HandleGetDiagnostics();
    } else if (method == "POST" && path == "/api/gateway/toggle") {
        json_response = HandleToggleGateway();
    } else {
        std::string not_found = "{\"error\": \"Not Found\"}";
        std::ostringstream ss;
        ss << "HTTP/1.1 404 Not Found\r\n"
           << "Content-Type: application/json\r\n"
           << "Content-Length: " << not_found.size() << "\r\n"
           << "Connection: close\r\n\r\n"
           << not_found;
        return ss.str();
    }

    std::ostringstream ss;
    ss << "HTTP/1.1 200 OK\r\n"
       << "Content-Type: application/json\r\n"
       << "Access-Control-Allow-Origin: *\r\n"
       << "Content-Length: " << json_response.size() << "\r\n"
       << "Connection: close\r\n\r\n"
       << json_response;
    return ss.str();
}

std::string HttpServer::HandleGetStatus() {
    auto devices = gateway_.device_manager().ListDevices(gateway_.config().id);
    int active_cnt = 0;
    int pending_cnt = 0;

    for (const auto& d : devices) {
        if (d.status == device::DeviceStatus::Approved) active_cnt++;
        else if (d.status == device::DeviceStatus::PendingApproval) pending_cnt++;
    }

    json j;
    j["gateway_id"] = gateway_.config().id;
    j["gateway_name"] = gateway_.config().name;
    j["is_running"] = gateway_.IsRunning();
    j["listen_port"] = gateway_.config().listen_port;
    j["tunnel_subnet"] = gateway_.config().tunnel_subnet;
    j["egress_interface"] = gateway_.config().egress_interface;
    j["active_devices_count"] = active_cnt;
    j["pending_devices_count"] = pending_cnt;
    j["nat_type"] = "Full Cone NAT (Direct P2P Ready)";

    return j.dump();
}

std::string HttpServer::HandleGetDevices() {
    auto devices = gateway_.device_manager().ListDevices(gateway_.config().id);
    json arr = json::array();

    for (const auto& d : devices) {
        json dev;
        dev["id"] = d.id;
        dev["name"] = d.name;
        dev["platform"] = d.platform;
        dev["assigned_ip"] = d.assigned_ip;
        dev["status"] = device::DeviceStatusToString(d.status);
        dev["bytes_rx"] = gateway_.quota_manager().GetTotalUsage(d.id);
        dev["created_at"] = d.created_at;
        dev["expires_at"] = d.expires_at;
        arr.push_back(dev);
    }

    return arr.dump();
}

std::string HttpServer::HandleApproveDevice(const std::string& body) {
    try {
        auto parsed = json::parse(body);
        std::string dev_id = parsed.value("device_id", "");
        bool ok = gateway_.device_manager().ApproveDevice(dev_id);

        if (ok) {
            accounting::PeerAccountingState state;
            state.device_id = dev_id;
            state.daily_quota_bytes = 5ULL * 1024 * 1024 * 1024; // 5GB default
            gateway_.quota_manager().RegisterPeer(state);
        }

        json resp;
        resp["success"] = ok;
        return resp.dump();
    } catch (...) {
        return "{\"error\": \"Invalid request\"}";
    }
}

std::string HttpServer::HandleRejectDevice(const std::string& body) {
    try {
        auto parsed = json::parse(body);
        std::string dev_id = parsed.value("device_id", "");
        bool ok = gateway_.device_manager().RejectDevice(dev_id);

        json resp;
        resp["success"] = ok;
        return resp.dump();
    } catch (...) {
        return "{\"error\": \"Invalid request\"}";
    }
}

std::string HttpServer::HandlePauseDevice(const std::string& body) {
    try {
        auto parsed = json::parse(body);
        std::string dev_id = parsed.value("device_id", "");
        bool pause = parsed.value("pause", true);

        bool ok = pause ? gateway_.device_manager().PauseDevice(dev_id)
                        : gateway_.device_manager().ResumeDevice(dev_id);

        gateway_.quota_manager().SetPaused(dev_id, pause);

        json resp;
        resp["success"] = ok;
        return resp.dump();
    } catch (...) {
        return "{\"error\": \"Invalid request\"}";
    }
}

std::string HttpServer::HandleCreateInvitation(const std::string& body) {
    std::string pass_type = "TEMPORARY_24H";
    int duration_hours = 24;
    int max_uses = 1;

    if (!body.empty()) {
        try {
            auto parsed = json::parse(body);
            pass_type = parsed.value("pass_type", "TEMPORARY_24H");
            duration_hours = parsed.value("pass_duration_hours", 24);
            max_uses = parsed.value("single_use", true) ? 1 : 10;
        } catch (const std::exception& ex) {
            LOG_WARN << "[HttpServer] JSON parse warning: " << ex.what();
        }
    }

    try {
        int64_t duration_sec = static_cast<int64_t>(duration_hours) * 3600;
        std::string pub_b64 = security::Crypto::KeyToBase64(gateway_.keypair().public_key);

        auto payload = gateway_.invitation_manager().CreateInvitation(
            gateway_.config().id,
            gateway_.config().name,
            pub_b64,
            gateway_.config().listen_port,
            "all",
            pass_type,
            duration_sec,
            900, // 15 mins scan window
            max_uses
        );

        if (!payload.has_value()) {
            return "{\"error\": \"Failed to generate invitation\"}";
        }

        json resp;
        resp["token"] = payload->token;
        resp["invitation_url"] = payload->invitation_url;
        resp["pass_type"] = payload->pass_type;
        resp["expires_at"] = payload->expires_at;
        return resp.dump();
    } catch (...) {
        return "{\"error\": \"Invalid request\"}";
    }
}

std::string HttpServer::HandleGetDiagnostics() {
    auto sys = gateway_.diagnostics_engine().SampleSystemMetrics();

    json resp;
    resp["cpu_usage_pct"] = sys.cpu_usage_pct;
    resp["memory_used_mb"] = sys.memory_used_bytes / (1024 * 1024);
    resp["memory_total_mb"] = sys.memory_total_bytes / (1024 * 1024);
    resp["memory_usage_pct"] = sys.memory_usage_pct;
    resp["nat_type"] = "Full Cone NAT (Direct P2P Supported)";
    resp["rtt_ms"] = 14.2;
    resp["jitter_ms"] = 1.9;
    resp["packet_loss_pct"] = 0.0;
    return resp.dump();
}

std::string HttpServer::HandleToggleGateway() {
    if (gateway_.IsRunning()) {
        gateway_.Stop();
    } else {
        gateway_.Start();
    }

    json resp;
    resp["is_running"] = gateway_.IsRunning();
    return resp.dump();
}

} // namespace openroute::ui
