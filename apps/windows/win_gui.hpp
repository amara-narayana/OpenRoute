#pragma once

#if defined(_WIN32)
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#endif

#include "core/gateway/gateway_server.hpp"
#include "core/ui/http_server.hpp"

#include <memory>
#include <string>

namespace openroute::gui {

class MainWindow {
public:
    MainWindow(gateway::GatewayServer& gateway, ui::HttpServer& http_server);
    ~MainWindow();

    bool Create();
    int RunMessageLoop();
    void Close();

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void InitControls(HWND hWnd);
    void SwitchTab(int tab_index);
    void RefreshDeviceList();
    void RefreshBusinessStats();
    void UpdateHeaderStatus();

    void OnApproveClicked();
    void OnPauseClicked();
    void OnHangUpClicked();
    void OnSaveRatesClicked();
    void OnGeneratePinClicked();
    void OnCopyCustomerMessageClicked();

    gateway::GatewayServer& gateway_;
    ui::HttpServer& http_server_;

    HWND hWnd_ = NULL;
    HWND hTab_ = NULL;
    HWND hHeader_ = NULL;

    // Tab 0: Calls & Devices
    HWND hList_ = NULL;
    HWND btnApprove_ = NULL;
    HWND btnPause_ = NULL;
    HWND btnHangUp_ = NULL;
    HWND btnRefresh_ = NULL;

    // Tab 1: Business & Passes
    HWND grpRates_ = NULL;
    HWND lblCurr_ = NULL;
    HWND editCurr_ = NULL;
    HWND lblRateGb_ = NULL;
    HWND editRateGb_ = NULL;
    HWND lblRateHr_ = NULL;
    HWND editRateHr_ = NULL;
    HWND btnSaveRates_ = NULL;

    HWND grpPassGen_ = NULL;
    HWND lblDur_ = NULL;
    HWND cmbDur_ = NULL;
    HWND lblQuota_ = NULL;
    HWND cmbQuota_ = NULL;
    HWND btnGenPin_ = NULL;
    HWND editGenPin_ = NULL;
    HWND btnCopyMsg_ = NULL;

    HWND grpStats_ = NULL;
    HWND lblLifetimeRev_ = NULL;
    HWND lblLifetimeGb_ = NULL;
    HWND lblTotalCalls_ = NULL;

    // Tab 2: Universal Guide
    HWND editGuide_ = NULL;

    int current_tab_ = 0;
    std::string latest_generated_pin_;
    std::string public_ip_cached_;
};

} // namespace openroute::gui
