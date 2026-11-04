#include "core/common/logger.hpp"
#include "core/security/crypto.hpp"
#include "core/storage/database.hpp"
#include "core/gateway/gateway_server.hpp"
#include "core/ui/http_server.hpp"
#include "apps/windows/win_gui.hpp"

#include <csignal>
#include <iostream>
#include <string_view>
#include <thread>
#include <chrono>

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#include <objbase.h>
#endif

namespace {
std::atomic<bool> g_shutdown_requested{false};

void SignalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        LOG_INFO << "Received termination signal. Shutting down gracefully...";
        g_shutdown_requested = true;
    }
}

#if defined(_WIN32)
BOOL WINAPI ConsoleCtrlHandler(DWORD ctrl_type) {
    switch (ctrl_type) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            LOG_INFO << "Received console termination event. Shutting down gracefully...";
            g_shutdown_requested = true;
            return TRUE;
        default:
            return FALSE;
    }
}
#endif
} // namespace

int main(int argc, char* argv[]) {
    using namespace openroute;

    std::cout << "========================================================\n"
              << "  OpenRoute Gateway v0.1.0\n"
              << "  Tagline: Your internet. Shared on your terms.\n"
              << "========================================================\n\n" << std::flush;

    common::Logger::Instance().SetMinLevel(common::LogLevel::Info);
    LOG_INFO << "Initializing OpenRoute Gateway Engine...";

    bool headless = false;
    bool native_gui = false;
    bool open_browser = false;
    uint16_t http_port = 8765;
    std::string db_path = "openroute.db";

    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "--version" || arg == "-v") {
            std::cout << "OpenRoute version 0.1.0 (Windows 11 x86_64)\n" << std::flush;
            return 0;
        }
        if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: openroute_gateway [options]\n"
                      << "  -v, --version       Show version\n"
                      << "  -h, --help          Show help\n"
                      << "  --headless, --cli   Run in console-only headless mode (no Windows GUI)\n"
                      << "  --native-gui        Launch classic Win32 GUI window\n"
                      << "  --open-browser      Open in default web browser tab\n"
                      << "  --db <path>         Specify custom database path\n"
                      << "  --port <port>       HTTP dashboard port (default: 8765)\n" << std::flush;
            return 0;
        }
        if (arg == "--headless" || arg == "--cli" || arg == "--no-gui") {
            headless = true;
        }
        if (arg == "--native-gui") {
            native_gui = true;
        }
        if (arg == "--open-browser") {
            open_browser = true;
        }
        if (arg == "--port" && i + 1 < argc) {
            http_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        }
        if (arg == "--db" && i + 1 < argc) {
            db_path = argv[++i];
        }
    }

    // Register signal and console handlers
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);
#if defined(_WIN32)
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
#endif

    try {
        // 1. Initialize SQLite Database
        storage::Database db;
        if (!db.Open(db_path)) {
            LOG_FATAL << "Failed to open local database: " << db_path;
            return 1;
        }

        if (!db.ApplyMigrations()) {
            LOG_FATAL << "Failed to apply database migrations.";
            return 1;
        }
        LOG_INFO << "Database migrations applied successfully. Schema version: " << db.GetSchemaVersion();

        // 2. Initialize Gateway Server
        gateway::GatewayConfig gw_config;
        gw_config.id = "gw_default";
        gw_config.name = "OpenRoute Windows Gateway";
        gw_config.listen_port = 51820;
        gw_config.tunnel_subnet = "10.77.0.0/24";
        gw_config.tunnel_ip = "10.77.0.1";
        gw_config.egress_interface = "Wi-Fi";

        gateway::GatewayServer gateway(db, gw_config);
        if (!gateway.Initialize()) {
            LOG_WARN << "Gateway running in userspace simulation mode (administrative NetNat requires elevated terminal).";
        }
        gateway.Start();

        // 3. Initialize & Start HTTP Dashboard Server
        ui::HttpServer http_server(gateway, http_port);
        if (!http_server.Start()) {
            LOG_ERROR << "Failed to start HTTP dashboard server on port " << http_port;
            return 1;
        }

        std::string dashboard_url = "http://127.0.0.1:" + std::to_string(http_port);
        LOG_INFO << "OpenRoute Dashboard ready at: " << dashboard_url;

#if defined(_WIN32)
        if (open_browser) {
            LOG_INFO << "Launching control dashboard in default browser...";
            CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
            ShellExecuteA(NULL, "open", dashboard_url.c_str(), NULL, NULL, SW_SHOWNORMAL);
        }

        std::cout << "\n[OK] OpenRoute Gateway is running.\n"
                  << "     Universal Proxy Port: 8080 (HTTP CONNECT / SOCKS5)\n"
                  << "     WireGuard UDP Port:   51820\n"
                  << "     Dashboard URL:        " << dashboard_url << "\n\n" << std::flush;

        if (!headless) {
            if (native_gui) {
                LOG_INFO << "Opening OpenRoute Classic Win32 GUI...";
                gui::MainWindow main_win(gateway, http_server);
                if (main_win.Create()) {
                    main_win.RunMessageLoop();
                    g_shutdown_requested = true;
                }
            } else {
                LOG_INFO << "Launching OpenRoute Modern Desktop Application Window...";
                CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

                // Launch in standalone App Window mode (no browser toolbar, no address bar, no tabs)
                std::string app_args = "--app=" + dashboard_url + " --window-size=1280,840";
                HINSTANCE hRes = ShellExecuteA(NULL, "open", "msedge.exe", app_args.c_str(), NULL, SW_SHOWNORMAL);
                if (reinterpret_cast<INT_PTR>(hRes) <= 32) {
                    std::string edge_path = "C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe";
                    hRes = ShellExecuteA(NULL, "open", edge_path.c_str(), app_args.c_str(), NULL, SW_SHOWNORMAL);
                    if (reinterpret_cast<INT_PTR>(hRes) <= 32) {
                        // Fallback to native Win32 GUI
                        gui::MainWindow main_win(gateway, http_server);
                        if (main_win.Create()) {
                            main_win.RunMessageLoop();
                            g_shutdown_requested = true;
                        }
                    }
                }

                while (!g_shutdown_requested) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(250));
                }
            }
        } else {
            std::cout << "Running in headless CLI mode. Press Ctrl+C to stop.\n\n" << std::flush;
            while (!g_shutdown_requested) {
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
        }
#else
        std::cout << "\n[OK] OpenRoute Gateway is running.\n"
                  << "     Dashboard URL: " << dashboard_url << "\n"
                  << "     Press Ctrl+C to stop.\n\n" << std::flush;

        while (!g_shutdown_requested) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
#endif

        LOG_INFO << "Stopping HTTP Dashboard Server...";
        http_server.Stop();

        LOG_INFO << "Stopping Gateway Engine...";
        gateway.Stop();

        LOG_INFO << "OpenRoute Gateway terminated cleanly.";
        return 0;
    } catch (const std::exception& ex) {
        LOG_FATAL << "Fatal exception in OpenRoute Gateway: " << ex.what();
        return 1;
    } catch (...) {
        LOG_FATAL << "Fatal unknown exception in OpenRoute Gateway.";
        return 1;
    }
}
