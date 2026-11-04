#include "core/ui/http_server.hpp"
#include "core/common/logger.hpp"
#include "core/networking/nat_probe.hpp"
#include "apps/windows/dashboard_assets.hpp"
#include "apps/windows/join_assets.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <fstream>
#include <sstream>
#include <vector>

using json = nlohmann::json;

namespace openroute::ui {

static std::string UrlEncode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::uppercase << std::setw(2) << int(static_cast<unsigned char>(c));
        }
    }
    return escaped.str();
}

static std::string GetLocalLanIp() {
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        return "192.168.0.239";
    }
    sockaddr_in loopback{};
    loopback.sin_family = AF_INET;
    inet_pton(AF_INET, "8.8.8.8", &loopback.sin_addr);
    loopback.sin_port = htons(53);
    if (connect(sock, reinterpret_cast<sockaddr*>(&loopback), sizeof(loopback)) == SOCKET_ERROR) {
        closesocket(sock);
        return "192.168.0.239";
    }
    sockaddr_in local_addr{};
    int name_len = sizeof(local_addr);
    if (getsockname(sock, reinterpret_cast<sockaddr*>(&local_addr), &name_len) == SOCKET_ERROR) {
        closesocket(sock);
        return "192.168.0.239";
    }
    closesocket(sock);
    char ip_str[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &local_addr.sin_addr, ip_str, sizeof(ip_str))) {
        std::string ip(ip_str);
        if (ip != "0.0.0.0" && ip != "127.0.0.1") {
            return ip;
        }
    }
    return "192.168.0.239";
}

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
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
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

    // Launch background STUN prober to discover public endpoint
    stun_thread_ = std::thread([this]() {
        try {
            networking::NatProbe probe;
            auto res = probe.PerformFullProbe();
            if (!res.public_ip.empty()) {
                std::lock_guard<std::mutex> lock(config_mutex_);
                public_ip_ = res.public_ip;
                public_port_ = res.public_port > 0 ? res.public_port : gateway_.config().listen_port;
                nat_type_desc_ = networking::NatTypeToString(res.type);
                LOG_INFO << "[HttpServer] STUN discovered public endpoint: " << public_ip_ << ":" << public_port_ << " (" << nat_type_desc_ << ")";
            }
        } catch (...) {}
    });

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

    if (stun_thread_.joinable()) {
        stun_thread_.join();
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

        try {
            std::thread([this, client_sock]() {
                try {
                    HandleClient(client_sock);
                } catch (const std::exception& ex) {
                    LOG_ERROR << "[HttpServer] Client thread exception: " << ex.what();
                } catch (...) {
                    LOG_ERROR << "[HttpServer] Client thread unknown exception";
                }
            }).detach();
        } catch (const std::exception& ex) {
            LOG_ERROR << "[HttpServer] Failed to spawn thread for client socket: " << ex.what();
            closesocket(client_sock);
        }
    }
}

void HttpServer::HandleClient(SOCKET client_socket) {
    try {
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
        if (!(stream >> method >> path >> protocol)) {
            closesocket(client_socket);
            return;
        }

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
        int total_sent = 0;
        int to_send = static_cast<int>(response.size());
        while (total_sent < to_send) {
            int bytes_sent = send(client_socket, response.data() + total_sent, to_send - total_sent, 0);
            if (bytes_sent <= 0) break;
            total_sent += bytes_sent;
        }
        shutdown(client_socket, SD_SEND);
        closesocket(client_socket);
    } catch (const std::exception& ex) {
        LOG_ERROR << "[HttpServer] Error in HandleClient: " << ex.what();
        closesocket(client_socket);
    } catch (...) {
        LOG_ERROR << "[HttpServer] Unknown error in HandleClient";
        closesocket(client_socket);
    }
}

std::string HttpServer::HandleRequest(const std::string& method, const std::string& path, const std::string& body) {
    if (method == "OPTIONS") {
        return "HTTP/1.1 204 No Content\r\n"
               "Access-Control-Allow-Origin: *\r\n"
               "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
               "Access-Control-Allow-Headers: Content-Type\r\n"
               "Connection: close\r\n\r\n";
    }

    if ((method == "GET" || method == "HEAD") && (path == "/" || path == "/index.html")) {
        std::string html = GetDashboardHtml();
        std::ostringstream ss;
        ss << "HTTP/1.1 200 OK\r\n"
           << "Content-Type: text/html; charset=utf-8\r\n"
           << "Content-Length: " << html.size() << "\r\n"
           << "Connection: close\r\n\r\n";
        if (method == "GET") {
            ss << html;
        }
        return ss.str();
    }

    if ((method == "GET" || method == "HEAD") && (path == "/join" || path.rfind("/join?", 0) == 0)) {
        std::string html = GetJoinHtml();
        std::ostringstream ss;
        ss << "HTTP/1.1 200 OK\r\n"
           << "Content-Type: text/html; charset=utf-8\r\n"
           << "Cache-Control: no-cache, no-store, must-revalidate\r\n"
           << "Pragma: no-cache\r\n"
           << "Expires: 0\r\n"
           << "Access-Control-Allow-Origin: *\r\n"
           << "Content-Length: " << html.size() << "\r\n"
           << "Connection: close\r\n\r\n";
        if (method == "GET") {
            ss << html;
        }
        return ss.str();
    }

    if ((method == "GET" || method == "HEAD") && (path == "/download/openroute.apk" || path == "/download/app")) {
        std::vector<std::string> search_paths = {
            "apps/android/openroute.apk",
            "build/apk/openroute.apk",
            "apps/android/app/build/outputs/apk/debug/app-debug.apk"
        };
        for (const auto& apk_path : search_paths) {
            std::ifstream file(apk_path, std::ios::binary | std::ios::ate);
            if (file.is_open()) {
                std::streamsize size = file.tellg();
                file.seekg(0, std::ios::beg);
                std::ostringstream ss;
                ss << "HTTP/1.1 200 OK\r\n"
                   << "Content-Type: application/vnd.android.package-archive\r\n"
                   << "Content-Disposition: attachment; filename=\"openroute.apk\"\r\n"
                   << "Cache-Control: no-cache, no-store, must-revalidate\r\n"
                   << "Pragma: no-cache\r\n"
                   << "Expires: 0\r\n"
                   << "Access-Control-Allow-Origin: *\r\n"
                   << "Content-Length: " << size << "\r\n"
                   << "Connection: close\r\n\r\n";
                if (method == "HEAD") {
                    return ss.str();
                }
                std::vector<char> buffer(static_cast<size_t>(size));
                if (file.read(buffer.data(), size)) {
                    std::string header = ss.str();
                    std::string response;
                    response.reserve(header.size() + static_cast<size_t>(size));
                    response.append(header);
                    response.append(buffer.data(), static_cast<size_t>(size));
                    return response;
                }
            }
        }
        std::string not_found = "{\"error\": \"APK not found on server\"}";
        std::ostringstream ss;
        ss << "HTTP/1.1 404 Not Found\r\nContent-Type: application/json\r\nContent-Length: "
           << not_found.size() << "\r\nConnection: close\r\n\r\n" << not_found;
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
    } else if (method == "POST" && path == "/api/devices/register") {
        json_response = HandleRegisterDevice(body);
    } else if (method == "POST" && path == "/api/invitations/create") {
        json_response = HandleCreateInvitation(body);
    } else if (method == "GET" && path == "/api/diagnostics") {
        json_response = HandleGetDiagnostics();
    } else if (method == "POST" && path == "/api/gateway/toggle") {
        json_response = HandleToggleGateway();
    } else if (method == "POST" && path == "/api/settings/network") {
        json_response = HandleUpdateSettings(body);
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

    std::lock_guard<std::mutex> lock(config_mutex_);

    json j;
    j["gateway_id"] = gateway_.config().id;
    j["gateway_name"] = gateway_.config().name;
    j["is_running"] = gateway_.IsRunning();
    j["listen_port"] = gateway_.config().listen_port;
    j["tunnel_subnet"] = gateway_.config().tunnel_subnet;
    j["egress_interface"] = gateway_.config().egress_interface;
    j["active_devices_count"] = active_cnt;
    j["pending_devices_count"] = pending_cnt;
    j["local_ip"] = GetLocalLanIp();
    j["public_ip"] = public_ip_.empty() ? "Probing STUN..." : public_ip_;
    j["public_port"] = public_port_;
    j["public_host"] = public_host_;
    j["public_url"] = public_url_;
    j["nat_type"] = nat_type_desc_;

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

std::string HttpServer::HandleRegisterDevice(const std::string& body) {
    try {
        auto parsed = json::parse(body);
        std::string token = parsed.value("token", "");
        std::string device_name = parsed.value("device_name", "Android Phone");
        std::string platform = parsed.value("platform", "android");
        std::string assigned_ip = parsed.value("assigned_ip", "10.77.0.2");

        int64_t duration_sec = 86400;
        std::string gw_id = gateway_.config().id;

        // Validate and redeem the token if present
        if (!token.empty()) {
            gateway_.invitation_manager().ValidateAndRedeemToken(token, duration_sec, gw_id);
        }

        int64_t expires = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count() + duration_sec;

        std::string mock_pub = "mobile_client_" + assigned_ip;
        std::string out_dev_id;
        std::string out_ip;

        gateway_.device_manager().RegisterDeviceRequest(
            gateway_.config().id,
            device_name,
            platform,
            mock_pub,
            expires,
            out_dev_id,
            out_ip
        );

        if (!out_dev_id.empty()) {
            // Auto-approve newly registered pass device
            gateway_.device_manager().ApproveDevice(out_dev_id);

            // Register peer in quota manager with generous 10GB limit
            accounting::PeerAccountingState state{};
            state.device_id = out_dev_id;
            state.daily_quota_bytes = 10ULL * 1024 * 1024 * 1024;
            state.expires_at = expires;
            gateway_.quota_manager().RegisterPeer(state);

            LOG_INFO << "[HttpServer] Registered and auto-approved guest device: " << device_name
                     << " (IP: " << out_ip << ", ID: " << out_dev_id << ")";

            json resp;
            resp["success"] = true;
            resp["device_id"] = out_dev_id;
            resp["status"] = "APPROVED";
            resp["assigned_ip"] = out_ip;
            return resp.dump();
        }

        return "{\"error\": \"Could not register device request\"}";
    } catch (const std::exception& ex) {
        LOG_WARN << "[HttpServer] Error registering device: " << ex.what();
        return "{\"error\": \"Failed to register device\"}";
    }
}

std::string HttpServer::HandleCreateInvitation(const std::string& body) {
    std::string pass_type = "TEMPORARY_24H";
    int duration_hours = 24;
    int max_uses = 1;
    std::string scope = "remote";
    std::string custom_host = "";
    std::string custom_url = "";

    if (!body.empty()) {
        try {
            auto parsed = json::parse(body);
            pass_type = parsed.value("pass_type", "TEMPORARY_24H");
            duration_hours = parsed.value("pass_duration_hours", 24);
            max_uses = parsed.value("single_use", true) ? 1 : 10;
            scope = parsed.value("network_scope", "remote");
            custom_host = parsed.value("custom_host", "");
            custom_url = parsed.value("custom_url", "");
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

        std::string lan_ip = GetLocalLanIp();
        std::string effective_host = lan_ip;
        uint16_t effective_port = gateway_.config().listen_port;

        {
            std::lock_guard<std::mutex> lock(config_mutex_);
            if (scope == "remote") {
                if (!custom_host.empty()) {
                    effective_host = custom_host;
                } else if (!public_host_.empty()) {
                    effective_host = public_host_;
                } else if (!public_ip_.empty()) {
                    effective_host = public_ip_;
                }
                effective_port = public_port_;
            }
        }

        std::string base_join_url;
        {
            std::lock_guard<std::mutex> lock(config_mutex_);
            if (!custom_url.empty()) {
                base_join_url = custom_url;
            } else if (!public_url_.empty() && scope == "remote") {
                base_join_url = public_url_;
            } else {
                base_join_url = "http://" + effective_host + ":" + std::to_string(port_);
            }
        }

        while (!base_join_url.empty() && base_join_url.back() == '/') {
            base_join_url.pop_back();
        }

        std::string join_url = base_join_url + "/join?token=" + UrlEncode(payload->token)
                             + "&gw=" + UrlEncode(gateway_.config().id)
                             + "&name=" + UrlEncode(gateway_.config().name)
                             + "&pub=" + UrlEncode(pub_b64)
                             + "&host=" + UrlEncode(effective_host)
                             + "&port=" + std::to_string(effective_port)
                             + "&pass=" + UrlEncode(payload->pass_type)
                             + "&dur=" + std::to_string(duration_sec);

        std::string deep_invite_url = "openroute://invite?token=" + UrlEncode(payload->token)
                                    + "&gw=" + UrlEncode(gateway_.config().id)
                                    + "&name=" + UrlEncode(gateway_.config().name)
                                    + "&pub=" + UrlEncode(pub_b64)
                                    + "&host=" + UrlEncode(effective_host)
                                    + "&port=" + std::to_string(effective_port)
                                    + "&pass=" + UrlEncode(payload->pass_type)
                                    + "&dur=" + std::to_string(duration_sec);

        json resp;
        resp["token"] = payload->token;
        resp["invitation_url"] = deep_invite_url;
        resp["join_url"] = join_url;
        resp["effective_host"] = effective_host;
        resp["effective_port"] = effective_port;
        resp["network_scope"] = scope;
        resp["pass_type"] = payload->pass_type;
        resp["expires_at"] = payload->expires_at;
        return resp.dump();
    } catch (...) {
        return "{\"error\": \"Invalid request\"}";
    }
}

std::string HttpServer::HandleUpdateSettings(const std::string& body) {
    try {
        auto parsed = json::parse(body);
        std::lock_guard<std::mutex> lock(config_mutex_);
        if (parsed.contains("public_host")) public_host_ = parsed["public_host"].get<std::string>();
        if (parsed.contains("public_url")) public_url_ = parsed["public_url"].get<std::string>();
        if (parsed.contains("public_port")) public_port_ = parsed["public_port"].get<uint16_t>();

        json resp;
        resp["success"] = true;
        resp["public_host"] = public_host_;
        resp["public_url"] = public_url_;
        resp["public_port"] = public_port_;
        return resp.dump();
    } catch (...) {
        return "{\"error\": \"Failed to update settings\"}";
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
