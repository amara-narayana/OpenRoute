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
    std::string HandleGetDiagnostics();
    std::string HandleToggleGateway();

    gateway::GatewayServer& gateway_;
    uint16_t port_;
    std::atomic<bool> is_running_{false};
    SOCKET listen_socket_{INVALID_SOCKET};
    std::thread server_thread_;
};

} // namespace openroute::ui
