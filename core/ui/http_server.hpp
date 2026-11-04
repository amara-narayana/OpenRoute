#pragma once

#include "core/gateway/gateway_server.hpp"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

namespace openroute::ui {

class HttpServer {
public:
    HttpServer(gateway::GatewayServer& gateway, uint16_t port = 8765);
    ~HttpServer();

    bool Start();
    void Stop();
    [[nodiscard]] bool IsRunning() const { return is_running_; }
    [[nodiscard]] uint16_t GetPort() const { return port_; }

private:
    void ServerLoop();
    void HandleClient(SOCKET client_socket);

    std::string HandleRequest(const std::string& method, const std::string& path, const std::string& body);

    // Endpoint handlers
    std::string HandleGetStatus();
    std::string HandleGetDevices();
    std::string HandleApproveDevice(const std::string& body);
    std::string HandleRejectDevice(const std::string& body);
    std::string HandlePauseDevice(const std::string& body);
    std::string HandleCreateInvitation(const std::string& body);
    std::string HandleRegisterDevice(const std::string& body);
    std::string HandleGetDiagnostics();
    std::string HandleToggleGateway();
    std::string HandleUpdateSettings(const std::string& body);

    // Call sessions & Monetization endpoints
    std::string HandleGetCalls();
    std::string HandleApproveCall(const std::string& body);
    std::string HandlePauseCall(const std::string& body);
    std::string HandleHangUpCall(const std::string& body);
    std::string HandleGeneratePin(const std::string& body);
    std::string HandleGetBillingRates();
    std::string HandleUpdateBillingRates(const std::string& body);
    std::string HandleGetBillingSummary();

    gateway::GatewayServer& gateway_;
    uint16_t port_;
    std::atomic<bool> is_running_{false};
    SOCKET listen_socket_{INVALID_SOCKET};
    std::thread server_thread_;
    std::thread stun_thread_;

    mutable std::mutex config_mutex_;
    std::string public_ip_;
    uint16_t public_port_{51820};
    std::string public_host_;
    std::string public_url_;
    std::string nat_type_desc_{"Discovering via STUN..."};
};

} // namespace openroute::ui
