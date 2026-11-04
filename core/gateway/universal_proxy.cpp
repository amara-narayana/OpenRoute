#include "core/gateway/universal_proxy.hpp"
#include "core/common/logger.hpp"
#include "core/security/crypto.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <random>
#include <sstream>
#include <vector>

namespace openroute::gateway {

namespace {

// Base64 decoder helper
std::string Base64Decode(const std::string& input) {
    static const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; ++i) T[static_cast<unsigned char>(chars[i])] = i;

    int val = 0, valb = -8;
    for (unsigned char c : input) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<char>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

} // namespace

UniversalProxy::UniversalProxy(accounting::BillingManager& billing_mgr,
                               accounting::QuotaManager& quota_mgr,
                               policy::InvitationManager& invitation_mgr,
                               UniversalProxyConfig config)
    : config_(config),
      billing_mgr_(billing_mgr),
      quota_mgr_(quota_mgr),
      invitation_mgr_(invitation_mgr) {}

UniversalProxy::~UniversalProxy() {
    Stop();
}

std::string UniversalProxy::GenerateDialPin(int length) {
    static const char digits[] = "0123456789";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 9);

    std::string pin;
    for (int i = 0; i < length; ++i) {
        pin += digits[dis(gen)];
    }
    return pin;
}

std::string UniversalProxy::CreateDialPass(
    const std::string& pin,
    uint64_t quota_bytes,
    int64_t duration_sec,
    const std::string& platform)
{
    // Start or register the session in billing manager
    std::string session_id = billing_mgr_.StartOrResumeCall(
        pin,
        "", // client IP initially unknown until connect
        platform,
        "Pass-" + pin,
        quota_bytes,
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count() + duration_sec
    );

    // Also register in QuotaManager
    accounting::PeerAccountingState qstate;
    qstate.device_id = session_id;
    qstate.daily_quota_bytes = quota_bytes;
    qstate.expires_at = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() + duration_sec;
    quota_mgr_.RegisterPeer(qstate);

    return session_id;
}

bool UniversalProxy::Start() {
    if (is_running_) return true;

    listen_sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock_ == INVALID_SOCKET) {
        LOG_ERROR << "[UniversalProxy] Failed to create socket: " << WSAGetLastError();
        return false;
    }

    int opt = 1;
    setsockopt(listen_sock_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(config_.port);

    if (bind(listen_sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        LOG_ERROR << "[UniversalProxy] Bind failed on port " << config_.port << ": " << WSAGetLastError();
        closesocket(listen_sock_);
        listen_sock_ = INVALID_SOCKET;
        return false;
    }

    if (listen(listen_sock_, 50) == SOCKET_ERROR) {
        LOG_ERROR << "[UniversalProxy] Listen failed on port " << config_.port;
        closesocket(listen_sock_);
        listen_sock_ = INVALID_SOCKET;
        return false;
    }

    // Set 500ms accept timeout so we can gracefully shutdown
    DWORD timeout_ms = 500;
    setsockopt(listen_sock_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));

    is_running_ = true;
    listen_thread_ = std::thread(&UniversalProxy::ListenLoop, this);
    LOG_INFO << "[UniversalProxy] Universal HTTP/HTTPS Proxy engine active on port " << config_.port;
    return true;
}

void UniversalProxy::Stop() {
    if (!is_running_) return;
    is_running_ = false;

    if (listen_sock_ != INVALID_SOCKET) {
        closesocket(listen_sock_);
        listen_sock_ = INVALID_SOCKET;
    }

    {
        std::lock_guard<std::mutex> lock(active_socks_mutex_);
        for (SOCKET s : client_sockets_) {
            closesocket(s);
        }
        client_sockets_.clear();
    }

    if (listen_thread_.joinable()) {
        listen_thread_.join();
    }
    LOG_INFO << "[UniversalProxy] Proxy engine stopped cleanly.";
}

void UniversalProxy::ListenLoop() {
    while (is_running_) {
        sockaddr_in client_addr{};
        int addr_len = sizeof(client_addr);
        SOCKET client_sock = accept(listen_sock_, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);

        if (client_sock == INVALID_SOCKET) {
            continue;
        }

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), ip_str, INET_ADDRSTRLEN);
        std::string client_ip(ip_str);

        {
            std::lock_guard<std::mutex> lock(active_socks_mutex_);
            client_sockets_.push_back(client_sock);
        }

        std::thread([this, client_sock, client_ip]() {
            HandleClientConnection(client_sock, client_ip);
            {
                std::lock_guard<std::mutex> lock(active_socks_mutex_);
                auto it = std::find(client_sockets_.begin(), client_sockets_.end(), client_sock);
                if (it != client_sockets_.end()) {
                    client_sockets_.erase(it);
                }
            }
        }).detach();
    }
}

void UniversalProxy::HandleClientConnection(SOCKET client_sock, const std::string& client_ip) {
    char buf[4096];
    int n = recv(client_sock, buf, sizeof(buf) - 1, 0);
    if (n <= 0) {
        closesocket(client_sock);
        return;
    }
    buf[n] = '\0';
    std::string header(buf, n);

    std::istringstream stream(header);
    std::string method, target, proto;
    if (!(stream >> method >> target >> proto)) {
        closesocket(client_sock);
        return;
    }

    // 1. Direct Web / Dial-Pad Browser Portal Requests
    if (method == "GET" && (target == "/" || target == "/dial" || target.rfind("/dial?", 0) == 0 ||
                            target == "/openroute.pac" || target.rfind("/api/dial", 0) == 0)) {
        std::string resp = HandleHttpDialPortal(method, target, header, client_ip);
        send(client_sock, resp.data(), static_cast<int>(resp.size()), 0);
        closesocket(client_sock);
        return;
    }

    // 2. HTTPS CONNECT Tunnel (e.g. CONNECT google.com:443 HTTP/1.1)
    if (method == "CONNECT") {
        std::string dial_pin, session_id;
        if (!AuthenticateRequest(header, client_ip, dial_pin, session_id)) {
            // Require Dial-In PIN Authentication
            std::string auth_req =
                "HTTP/1.1 407 Proxy Authentication Required\r\n"
                "Proxy-Authenticate: Basic realm=\"OpenRoute Gateway (Enter Dial PIN as Password)\"\r\n"
                "Content-Type: text/plain; charset=utf-8\r\n"
                "Content-Length: 53\r\n"
                "Connection: close\r\n\r\n"
                "Authentication required. Enter your OpenRoute Dial PIN.";
            send(client_sock, auth_req.data(), static_cast<int>(auth_req.size()), 0);
            closesocket(client_sock);
            return;
        }

        // Parse host:port
        size_t colon_pos = target.find(':');
        std::string target_host = (colon_pos != std::string::npos) ? target.substr(0, colon_pos) : target;
        uint16_t target_port = 443;
        if (colon_pos != std::string::npos) {
            try {
                target_port = static_cast<uint16_t>(std::stoi(target.substr(colon_pos + 1)));
            } catch (...) {}
        }

        HandleConnectTunnel(client_sock, target_host, target_port, session_id);
        return;
    }

    // 3. Regular HTTP GET/POST Proxy Forwarding
    closesocket(client_sock);
}

bool UniversalProxy::AuthenticateRequest(const std::string& req_header,
                                        const std::string& client_ip,
                                        std::string& out_dial_pin,
                                        std::string& out_session_id)
{
    // Check if this IP already has an active, approved call session!
    auto existing_by_ip = billing_mgr_.FindActiveCallByIp(client_ip);
    if (existing_by_ip && existing_by_ip->is_approved && !existing_by_ip->is_paused && !existing_by_ip->is_hung_up) {
        out_dial_pin = existing_by_ip->dial_pin;
        out_session_id = existing_by_ip->session_id;
        return true;
    }

    // Check for Proxy-Authorization header
    size_t pos = req_header.find("Proxy-Authorization:");
    if (pos == std::string::npos) {
        pos = req_header.find("proxy-authorization:");
    }

    if (pos != std::string::npos) {
        size_t end_line = req_header.find("\r\n", pos);
        std::string line = req_header.substr(pos, end_line - pos);
        size_t basic_pos = line.find("Basic ");
        if (basic_pos != std::string::npos) {
            std::string b64 = line.substr(basic_pos + 6);
            // remove whitespace
            b64.erase(std::remove_if(b64.begin(), b64.end(), ::isspace), b64.end());
            std::string user_pass = Base64Decode(b64);
            size_t c_pos = user_pass.find(':');
            std::string user = (c_pos != std::string::npos) ? user_pass.substr(0, c_pos) : user_pass;
            std::string pass = (c_pos != std::string::npos) ? user_pass.substr(c_pos + 1) : "";

            // The user can enter PIN as pass or as user
            std::string pin = !pass.empty() ? pass : user;
            if (!pin.empty()) {
                auto call = billing_mgr_.FindActiveCallByPin(pin);
                if (call && call->is_approved && !call->is_paused && !call->is_hung_up) {
                    out_dial_pin = pin;
                    out_session_id = call->session_id;
                    return true;
                }

                // If not found in active calls, auto-start a call for this PIN if valid
                out_session_id = billing_mgr_.StartOrResumeCall(pin, client_ip, "Proxy-Device", "Client-" + pin);
                out_dial_pin = pin;
                return true;
            }
        }
    }

    // If no auth header, check if we allow open mode (if require_auth is false)
    if (!config_.require_auth) {
        out_dial_pin = "guest";
        out_session_id = billing_mgr_.StartOrResumeCall("guest", client_ip, "Guest", "Guest-" + client_ip);
        return true;
    }

    return false;
}

void UniversalProxy::HandleConnectTunnel(SOCKET client_sock,
                                        const std::string& target_host,
                                        uint16_t target_port,
                                        const std::string& session_id)
{
    // Resolve target
    addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    std::string port_str = std::to_string(target_port);
    if (getaddrinfo(target_host.c_str(), port_str.c_str(), &hints, &res) != 0 || !res) {
        std::string err_resp = "HTTP/1.1 502 Bad Gateway\r\nConnection: close\r\n\r\n";
        send(client_sock, err_resp.data(), static_cast<int>(err_resp.size()), 0);
        closesocket(client_sock);
        return;
    }

    SOCKET remote_sock = INVALID_SOCKET;
    for (auto* ptr = res; ptr != nullptr; ptr = ptr->ai_next) {
        remote_sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (remote_sock == INVALID_SOCKET) continue;

        if (connect(remote_sock, ptr->ai_addr, static_cast<int>(ptr->ai_addrlen)) == 0) {
            break; // Successfully connected!
        }
        closesocket(remote_sock);
        remote_sock = INVALID_SOCKET;
    }
    freeaddrinfo(res);

    if (remote_sock == INVALID_SOCKET) {
        std::string err_resp = "HTTP/1.1 504 Gateway Timeout\r\nConnection: close\r\n\r\n";
        send(client_sock, err_resp.data(), static_cast<int>(err_resp.size()), 0);
        closesocket(client_sock);
        return;
    }

    // Tunnel established! Notify client
    std::string ok_resp = "HTTP/1.1 200 Connection Established\r\n\r\n";
    send(client_sock, ok_resp.data(), static_cast<int>(ok_resp.size()), 0);

    // Full duplex streaming loop
    char buffer[16384];
    fd_set read_fds;
    while (is_running_) {
        // Check session validity in billing manager
        auto call = billing_mgr_.GetCall(session_id);
        if (!call || call->is_hung_up || call->is_paused || !call->is_approved) {
            break; // Call ended or hung up
        }

        FD_ZERO(&read_fds);
        FD_SET(client_sock, &read_fds);
        FD_SET(remote_sock, &read_fds);

        timeval tv{1, 0}; // 1 second timeout
        int sel = select(0, &read_fds, nullptr, nullptr, &tv);
        if (sel <= 0) {
            if (sel < 0) break; // Error
            continue;
        }

        // Client -> Remote
        if (FD_ISSET(client_sock, &read_fds)) {
            int bytes = recv(client_sock, buffer, sizeof(buffer), 0);
            if (bytes <= 0) break;
            int sent = send(remote_sock, buffer, bytes, 0);
            if (sent <= 0) break;
            billing_mgr_.RecordTraffic(session_id, 0, sent);
        }

        // Remote -> Client
        if (FD_ISSET(remote_sock, &read_fds)) {
            int bytes = recv(remote_sock, buffer, sizeof(buffer), 0);
            if (bytes <= 0) break;
            int sent = send(client_sock, buffer, bytes, 0);
            if (sent <= 0) break;
            billing_mgr_.RecordTraffic(session_id, sent, 0);
        }
    }

    closesocket(remote_sock);
    closesocket(client_sock);
}

std::string UniversalProxy::HandleHttpDialPortal(const std::string& /*method*/,
                                                 const std::string& path,
                                                 const std::string& /*body*/,
                                                 const std::string& client_ip)
{
    if (path == "/openroute.pac") {
        std::string pac = GetPacFileContent();
        std::ostringstream ss;
        ss << "HTTP/1.1 200 OK\r\n"
           << "Content-Type: application/x-ns-proxy-autoconfig\r\n"
           << "Content-Length: " << pac.size() << "\r\n"
           << "Connection: close\r\n\r\n" << pac;
        return ss.str();
    }

    // Dial Connect API
    if (path.rfind("/api/dial/connect?pin=", 0) == 0) {
        std::string pin = path.substr(22);
        size_t amp = pin.find('&');
        if (amp != std::string::npos) pin = pin.substr(0, amp);

        std::string sess_id = billing_mgr_.StartOrResumeCall(pin, client_ip, "Mobile-Phone", "DialPad-" + pin);
        std::string json = "{\"status\":\"CONNECTED\",\"session_id\":\"" + sess_id + "\",\"pin\":\"" + pin + "\"}";
        std::ostringstream ss;
        ss << "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " << json.size() << "\r\n\r\n" << json;
        return ss.str();
    }

    // Hangup API
    if (path.rfind("/api/dial/hangup?pin=", 0) == 0) {
        std::string pin = path.substr(21);
        auto call = billing_mgr_.FindActiveCallByPin(pin);
        if (call) {
            billing_mgr_.HangUpCall(call->session_id);
        }
        std::string json = "{\"status\":\"HUNG_UP\"}";
        std::ostringstream ss;
        ss << "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " << json.size() << "\r\n\r\n" << json;
        return ss.str();
    }

    // Status API
    if (path.rfind("/api/dial/status?pin=", 0) == 0) {
        std::string pin = path.substr(21);
        auto call = billing_mgr_.FindActiveCallByPin(pin);
        std::ostringstream json;
        if (call && !call->is_hung_up) {
            json << "{\"connected\":true,\"bytes_used\":" << (call->bytes_rx + call->bytes_tx)
                 << ",\"formatted_bytes\":\"" << accounting::BillingManager::FormatBytes(call->bytes_rx + call->bytes_tx)
                 << "\",\"duration\":" << (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count() - call->start_time)
                 << ",\"billed\":\"" << accounting::BillingManager::FormatCurrency(call->billed_amount)
                 << "\"}";
        } else {
            json << "{\"connected\":false}";
        }
        std::string jstr = json.str();
        std::ostringstream ss;
        ss << "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " << jstr.size() << "\r\n\r\n" << jstr;
        return ss.str();
    }

    // Serve Interactive Phone Dial-Pad
    std::string html = GetDialPadHtml(client_ip);
    std::ostringstream ss;
    ss << "HTTP/1.1 200 OK\r\n"
       << "Content-Type: text/html; charset=utf-8\r\n"
       << "Content-Length: " << html.size() << "\r\n"
       << "Connection: close\r\n\r\n" << html;
    return ss.str();
}

std::string UniversalProxy::GetPacFileContent() {
    std::string host = !config_.public_host.empty() ? config_.public_host : "127.0.0.1";
    std::ostringstream ss;
    ss << "function FindProxyForURL(url, host) {\n"
       << "  return \"PROXY " << host << ":" << config_.port << "; DIRECT\";\n"
       << "}\n";
    return ss.str();
}

std::string UniversalProxy::GetDialPadHtml(const std::string& /*client_ip*/) {
    return R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>OpenRoute Phone Dial-In Internet</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
    body { background: #0f172a; color: #f8fafc; display: flex; flex-direction: column; align-items: center; justify-content: center; min-height: 100vh; padding: 16px; }
    .phone-container { width: 100%; max-width: 360px; background: #1e293b; border-radius: 28px; padding: 24px 20px; box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.5); border: 1px solid #334155; }
    .header { text-align: center; margin-bottom: 20px; }
    .badge { display: inline-flex; align-items: center; gap: 6px; padding: 4px 12px; background: #064e3b; color: #34d399; border-radius: 20px; font-size: 12px; font-weight: 600; margin-bottom: 8px; }
    .pulse-dot { width: 8px; height: 8px; background: #34d399; border-radius: 50%; animation: pulse 1.5s infinite; }
    @keyframes pulse { 0% { opacity: 1; transform: scale(1); } 50% { opacity: 0.4; transform: scale(1.2); } 100% { opacity: 1; transform: scale(1); } }
    .title { font-size: 20px; font-weight: 700; color: #fff; }
    .subtitle { font-size: 13px; color: #94a3b8; margin-top: 4px; }
    .screen { background: #0f172a; border-radius: 16px; padding: 18px; text-align: center; margin-bottom: 20px; border: 1px solid #334155; min-height: 90px; display: flex; flex-direction: column; justify-content: center; }
    .number-display { font-size: 30px; font-weight: 700; letter-spacing: 4px; color: #38bdf8; min-height: 38px; word-break: break-all; }
    .status-text { font-size: 13px; color: #64748b; margin-top: 4px; font-weight: 500; }
    .keypad { display: grid; grid-template-columns: repeat(3, 1fr); gap: 14px; margin-bottom: 20px; }
    .key { background: #334155; border: none; border-radius: 50%; width: 72px; height: 72px; margin: 0 auto; color: #fff; font-size: 24px; font-weight: 600; display: flex; flex-direction: column; align-items: center; justify-content: center; cursor: pointer; transition: all 0.15s ease; -webkit-tap-highlight-color: transparent; }
    .key:active { background: #475569; transform: scale(0.92); }
    .key-sub { font-size: 9px; color: #94a3b8; font-weight: 400; letter-spacing: 1px; margin-top: 1px; }
    .actions { display: flex; justify-content: center; gap: 20px; align-items: center; }
    .call-btn { background: #10b981; border: none; border-radius: 50%; width: 76px; height: 76px; color: white; display: flex; align-items: center; justify-content: center; cursor: pointer; box-shadow: 0 10px 15px -3px rgba(16, 185, 129, 0.4); transition: all 0.2s ease; }
    .call-btn:active { transform: scale(0.92); }
    .call-btn svg { width: 34px; height: 34px; fill: white; }
    .hangup-btn { background: #ef4444; border: none; border-radius: 50%; width: 76px; height: 76px; color: white; display: none; align-items: center; justify-content: center; cursor: pointer; box-shadow: 0 10px 15px -3px rgba(239, 68, 68, 0.4); }
    .hangup-btn svg { width: 34px; height: 34px; fill: white; }
    .backspace-btn { background: transparent; border: none; color: #94a3b8; font-size: 20px; cursor: pointer; width: 50px; height: 50px; display: flex; align-items: center; justify-content: center; }
    .stats-card { display: none; background: #0f172a; border-radius: 12px; padding: 12px 16px; margin-top: 16px; border: 1px solid #10b981; }
    .stats-row { display: flex; justify-content: space-between; font-size: 13px; margin: 4px 0; color: #cbd5e1; }
    .stats-val { font-weight: 700; color: #34d399; }
    .notice { font-size: 11px; text-align: center; color: #64748b; margin-top: 16px; }
  </style>
</head>
<body>
  <div class="phone-container">
    <div class="header">
      <div class="badge"><div class="pulse-dot"></div> OPENROUTE 5G GATEWAY</div>
      <div class="title">Dial-In Internet Access</div>
      <div class="subtitle">Dial pass PIN to route high-speed internet</div>
    </div>

    <div class="screen">
      <div class="number-display" id="display">--- ---</div>
      <div class="status-text" id="status">Enter your 6-digit dial PIN</div>
    </div>

    <div class="keypad" id="keypad">
      <button class="key" onclick="press('1')">1<span class="key-sub">&nbsp;</span></button>
      <button class="key" onclick="press('2')">2<span class="key-sub">ABC</span></button>
      <button class="key" onclick="press('3')">3<span class="key-sub">DEF</span></button>
      <button class="key" onclick="press('4')">4<span class="key-sub">GHI</span></button>
      <button class="key" onclick="press('5')">5<span class="key-sub">JKL</span></button>
      <button class="key" onclick="press('6')">6<span class="key-sub">MNO</span></button>
      <button class="key" onclick="press('7')">7<span class="key-sub">PQRS</span></button>
      <button class="key" onclick="press('8')">8<span class="key-sub">TUV</span></button>
      <button class="key" onclick="press('9')">9<span class="key-sub">WXYZ</span></button>
      <button class="key" onclick="press('*')">*<span class="key-sub">&nbsp;</span></button>
      <button class="key" onclick="press('0')">0<span class="key-sub">+</span></button>
      <button class="key" onclick="press('#')">#<span class="key-sub">&nbsp;</span></button>
    </div>

    <div class="actions">
      <button class="backspace-btn" onclick="backspace()">&#x232B;</button>
      <button class="call-btn" id="callBtn" onclick="dialCall()">
        <svg viewBox="0 0 24 24"><path d="M6.62 10.79a15.05 15.05 0 006.59 6.59l2.2-2.2c.27-.27.67-.36 1.02-.24 1.12.37 2.33.57 3.57.57.55 0 1 .45 1 1V20c0 .55-.45 1-1 1-9.39 0-17-7.61-17-17 0-.55.45-1 1-1h3.5c.55 0 1 .45 1 1 0 1.25.2 2.45.57 3.57.11.35.03.74-.25 1.02l-2.2 2.2z"/></svg>
      </button>
      <button class="hangup-btn" id="hangupBtn" onclick="hangUp()">
        <svg viewBox="0 0 24 24"><path d="M12 9c-1.6 0-3.15.25-4.6.72v3.1c0 .39-.23.74-.56.9-.98.49-1.87 1.12-2.66 1.85-.18.18-.43.28-.7.28-.28 0-.53-.11-.71-.29L.29 13.08a.996.996 0 010-1.41C3.28 8.84 7.42 7 12 7c4.58 0 8.72 1.84 11.71 4.67.39.39.39 1.02 0 1.41l-2.48 2.48c-.18.18-.43.29-.71.29-.27 0-.52-.11-.7-.28-.79-.74-1.69-1.36-2.67-1.85-.33-.16-.56-.5-.56-.9v-3.1C15.15 9.25 13.6 9 12 9z"/></svg>
      </button>
      <div style="width: 50px;"></div>
    </div>

    <div class="stats-card" id="statsCard">
      <div class="stats-row"><span>Status:</span><span class="stats-val" id="statStatus">Connected (In Call)</span></div>
      <div class="stats-row"><span>Data Streamed:</span><span class="stats-val" id="statData">0.0 MB</span></div>
      <div class="stats-row"><span>Call Duration:</span><span class="stats-val" id="statDuration">00:00</span></div>
      <div class="stats-row"><span>Billed Amount:</span><span class="stats-val" id="statBilled">₹ 0.00</span></div>
    </div>

    <div class="notice">
      Connected phone receives host 5G/Wi-Fi bandwidth until you tap Hang Up.
    </div>
  </div>

  <script>
    let currentPin = '';
    let isConnected = false;
    let pollInterval = null;

    // Check if URL has token or pin
    const urlParams = new URLSearchParams(window.location.search);
    const pinParam = urlParams.get('pin') || urlParams.get('token');
    if (pinParam) {
      currentPin = pinParam.replace(/[^0-9]/g, '');
      updateDisplay();
    }

    function press(digit) {
      if (isConnected) return;
      if (currentPin.length < 10) {
        currentPin += digit;
        updateDisplay();
      }
    }

    function backspace() {
      if (isConnected) return;
      if (currentPin.length > 0) {
        currentPin = currentPin.slice(0, -1);
        updateDisplay();
      }
    }

    function updateDisplay() {
      const disp = document.getElementById('display');
      if (currentPin.length === 0) {
        disp.innerText = '--- ---';
      } else {
        disp.innerText = currentPin;
      }
    }

    async function dialCall() {
      if (!currentPin || currentPin.length < 3) {
        alert('Please enter a valid numeric Dial PIN');
        return;
      }

      document.getElementById('status').innerText = 'Connecting to OpenRoute Gateway...';
      try {
        const resp = await fetch('/api/dial/connect?pin=' + encodeURIComponent(currentPin));
        const data = await resp.json();
        if (data.status === 'CONNECTED') {
          isConnected = true;
          document.getElementById('status').innerText = 'CALL CONNECTED - 5G INTERNET ACTIVE';
          document.getElementById('status').style.color = '#34d399';
          document.getElementById('callBtn').style.display = 'none';
          document.getElementById('hangupBtn').style.display = 'flex';
          document.getElementById('statsCard').style.display = 'block';

          // Start polling stats
          pollInterval = setInterval(fetchStats, 1000);
        } else {
          alert('Could not connect. Invalid PIN or pass expired.');
        }
      } catch (err) {
        alert('Failed to connect: ' + err.message);
      }
    }

    async function hangUp() {
      try {
        await fetch('/api/dial/hangup?pin=' + encodeURIComponent(currentPin));
      } catch (e) {}

      isConnected = false;
      clearInterval(pollInterval);
      document.getElementById('status').innerText = 'Call ended. Internet disconnected.';
      document.getElementById('status').style.color = '#ef4444';
      document.getElementById('statStatus').innerText = 'Disconnected';
      document.getElementById('statStatus').style.color = '#ef4444';
      document.getElementById('callBtn').style.display = 'flex';
      document.getElementById('hangupBtn').style.display = 'none';
    }

    async function fetchStats() {
      if (!isConnected) return;
      try {
        const resp = await fetch('/api/dial/status?pin=' + encodeURIComponent(currentPin));
        const data = await resp.json();
        if (data.connected) {
          document.getElementById('statData').innerText = data.formatted_bytes;
          const mins = Math.floor(data.duration / 60);
          const secs = data.duration % 60;
          document.getElementById('statDuration').innerText =
            String(mins).padStart(2, '0') + ':' + String(secs).padStart(2, '0');
          document.getElementById('statBilled').innerText = data.billed;
        } else {
          hangUp();
        }
      } catch (e) {}
    }
  </script>
</body>
</html>
)HTML";
}

} // namespace openroute::gateway
