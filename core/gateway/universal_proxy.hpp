#pragma once

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "core/accounting/billing_manager.hpp"
#include "core/accounting/quota_manager.hpp"
#include "core/policy/invitation_manager.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace openroute::gateway {

struct UniversalProxyConfig {
    uint16_t port = 8080;
    bool require_auth = true;
    std::string public_host = "";
};

class UniversalProxy {
public:
    UniversalProxy(accounting::BillingManager& billing_mgr,
                   accounting::QuotaManager& quota_mgr,
                   policy::InvitationManager& invitation_mgr,
                   UniversalProxyConfig config = {});
    ~UniversalProxy();

    bool Start();
    void Stop();

    [[nodiscard]] bool IsRunning() const { return is_running_; }
    [[nodiscard]] uint16_t GetPort() const { return config_.port; }
    void SetPublicHost(const std::string& host) { config_.public_host = host; }

    // Generates a new easy-to-dial numeric PIN (e.g. "849201" or "982-140-5501")
    std::string GenerateDialPin(int length = 6);

    // Creates an access pass with a numeric Dial PIN
    std::string CreateDialPass(const std::string& pin,
                               uint64_t quota_bytes = 0,
                               int64_t duration_sec = 86400,
                               const std::string& platform = "Universal");

private:
    void ListenLoop();
    void HandleClientConnection(SOCKET client_sock, const std::string& client_ip);

    bool AuthenticateRequest(const std::string& req_header,
                             const std::string& client_ip,
                             std::string& out_dial_pin,
                             std::string& out_session_id);

    void HandleConnectTunnel(SOCKET client_sock,
                             const std::string& target_host,
                             uint16_t target_port,
                             const std::string& session_id);

    std::string HandleHttpDialPortal(const std::string& method,
                                     const std::string& path,
                                     const std::string& body,
                                     const std::string& client_ip);

    std::string GetDialPadHtml(const std::string& client_ip);
    std::string GetPacFileContent();

    UniversalProxyConfig config_;
    accounting::BillingManager& billing_mgr_;
    accounting::QuotaManager& quota_mgr_;
    policy::InvitationManager& invitation_mgr_;

    std::atomic<bool> is_running_{false};
    SOCKET listen_sock_{INVALID_SOCKET};
    std::thread listen_thread_;

    mutable std::mutex active_socks_mutex_;
    std::vector<SOCKET> client_sockets_;
};

} // namespace openroute::gateway
