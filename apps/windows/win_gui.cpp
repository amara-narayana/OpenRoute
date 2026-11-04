#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "apps/windows/win_gui.hpp"
#include "core/common/logger.hpp"
#include "core/networking/nat_probe.hpp"

#include <commctrl.h>
#include <iomanip>
#include <sstream>
#include <vector>

#pragma comment(lib, "comctl32.lib")

#define TO_HMENU(id) reinterpret_cast<HMENU>(static_cast<INT_PTR>(id))

namespace openroute::gui {

namespace {
constexpr UINT_PTR IDT_REFRESH_TIMER = 1001;

constexpr int IDC_TAB_CONTROL = 2001;
constexpr int IDC_DEVICE_LIST = 2002;
constexpr int IDC_BTN_APPROVE = 2003;
constexpr int IDC_BTN_PAUSE   = 2004;
constexpr int IDC_BTN_HANGUP  = 2005;
constexpr int IDC_BTN_REFRESH = 2006;

constexpr int IDC_EDIT_CURR   = 2011;
constexpr int IDC_EDIT_RATEGB = 2012;
constexpr int IDC_EDIT_RATEHR = 2013;
constexpr int IDC_BTN_SAVERATES = 2014;

constexpr int IDC_CMB_DUR     = 2021;
constexpr int IDC_CMB_QUOTA   = 2022;
constexpr int IDC_BTN_GENPIN  = 2023;
constexpr int IDC_EDIT_GENPIN = 2024;
constexpr int IDC_BTN_COPYMSG = 2025;

constexpr int IDC_EDIT_GUIDE  = 2031;

std::wstring ToWString(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), nullptr, 0);
    std::wstring wstr(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), &wstr[0], size);
    return wstr;
}

std::string ToString(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
    std::string str(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), &str[0], size, nullptr, nullptr);
    return str;
}

void SetWindowTextA_UTF8(HWND hWnd, const std::string& text) {
    std::wstring w = ToWString(text);
    SetWindowTextW(hWnd, w.c_str());
}

HFONT GetDefaultSegoeFont(int height = 15, int weight = FW_NORMAL) {
    return CreateFontW(
        height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"
    );
}

} // namespace

MainWindow::MainWindow(gateway::GatewayServer& gateway, ui::HttpServer& http_server)
    : gateway_(gateway), http_server_(http_server) {}

MainWindow::~MainWindow() {
    Close();
}

void MainWindow::Close() {
    if (hWnd_) {
        DestroyWindow(hWnd_);
        hWnd_ = NULL;
    }
}

bool MainWindow::Create() {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    HINSTANCE hInstance = GetModuleHandle(NULL);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWindow::WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"OpenRouteGatewayManagerClass";

    RegisterClassExW(&wc);

    // Center window on screen
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int winW = 1040;
    int winH = 700;
    int posX = (screenW - winW) / 2;
    int posY = (screenH - winH) / 2;

    hWnd_ = CreateWindowExW(
        0,
        L"OpenRouteGatewayManagerClass",
        L"OpenRoute Gateway - Internet Sharing & Monetization Manager",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
        posX, posY, winW, winH,
        NULL, NULL, hInstance, this
    );

    if (!hWnd_) {
        LOG_ERROR << "[GUI] Failed to create main application window";
        return false;
    }

    ShowWindow(hWnd_, SW_SHOW);
    UpdateWindow(hWnd_);

    // Set 1-second refresh timer for live stats & active calls
    SetTimer(hWnd_, IDT_REFRESH_TIMER, 1000, NULL);

    return true;
}

int MainWindow::RunMessageLoop() {
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK MainWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MainWindow* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<MainWindow*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->hWnd_ = hWnd;
    } else {
        pThis = reinterpret_cast<MainWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (pThis) {
        return pThis->HandleMessage(hWnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT MainWindow::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            InitControls(hWnd);
            RefreshBusinessStats();
            RefreshDeviceList();
            UpdateHeaderStatus();
            return 0;
        }

        case WM_TIMER: {
            if (wParam == IDT_REFRESH_TIMER) {
                UpdateHeaderStatus();
                if (current_tab_ == 0) {
                    RefreshDeviceList();
                } else if (current_tab_ == 1) {
                    RefreshBusinessStats();
                }
            }
            return 0;
        }

        case WM_NOTIFY: {
            NMHDR* pnm = reinterpret_cast<NMHDR*>(lParam);
            if (pnm->idFrom == IDC_TAB_CONTROL && pnm->code == TCN_SELCHANGE) {
                int selected = TabCtrl_GetCurSel(hTab_);
                SwitchTab(selected);
            } else if (pnm->idFrom == IDC_DEVICE_LIST) {
                if (pnm->code == NM_CLICK) {
                    NMITEMACTIVATE* pia = reinterpret_cast<NMITEMACTIVATE*>(lParam);
                    if (pia && pia->iItem >= 0) {
                        wchar_t buf[64]{};
                        ListView_GetItemText(hList_, pia->iItem, 1, buf, 64);
                        selected_pin_ = ToString(buf);
                    }
                } else if (pnm->code == LVN_ITEMCHANGED) {
                    NMLISTVIEW* pnmv = reinterpret_cast<NMLISTVIEW*>(lParam);
                    if (pnmv && (pnmv->uNewState & LVIS_SELECTED) && pnmv->iItem >= 0) {
                        wchar_t buf[64]{};
                        ListView_GetItemText(hList_, pnmv->iItem, 1, buf, 64);
                        selected_pin_ = ToString(buf);
                    }
                }
            }
            return 0;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            switch (wmId) {
                case IDC_BTN_APPROVE:   OnApproveClicked(); break;
                case IDC_BTN_PAUSE:     OnPauseClicked(); break;
                case IDC_BTN_HANGUP:    OnHangUpClicked(); break;
                case IDC_BTN_REFRESH:   RefreshDeviceList(); break;
                case IDC_BTN_SAVERATES: OnSaveRatesClicked(); break;
                case IDC_BTN_GENPIN:    OnGeneratePinClicked(); break;
                case IDC_BTN_COPYMSG:   OnCopyCustomerMessageClicked(); break;
            }
            return 0;
        }

        case WM_DESTROY: {
            KillTimer(hWnd, IDT_REFRESH_TIMER);
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void MainWindow::InitControls(HWND hWnd) {
    HINSTANCE hInst = GetModuleHandle(NULL);
    HFONT hFontNormal = GetDefaultSegoeFont(15, FW_NORMAL);
    HFONT hFontBold = GetDefaultSegoeFont(15, FW_BOLD);
    HFONT hFontHeader = GetDefaultSegoeFont(18, FW_BOLD);

    // 1. Top Header Status Bar
    hHeader_ = CreateWindowExW(
        0, L"STATIC",
        L"OpenRoute Gateway: Initializing 5G/Wi-Fi Engine...",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOTIFY,
        20, 15, 990, 48, hWnd, NULL, hInst, NULL
    );
    SendMessage(hHeader_, WM_SETFONT, (WPARAM)hFontHeader, TRUE);

    // 2. Main Tab Control
    hTab_ = CreateWindowExW(
        0, WC_TABCONTROLW, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        20, 70, 990, 570, hWnd, TO_HMENU(IDC_TAB_CONTROL), hInst, NULL
    );
    SendMessage(hTab_, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

    TCITEMW tie{};
    tie.mask = TCIF_TEXT;
    tie.pszText = const_cast<LPWSTR>(L"  Live Calls & Devices  ");
    TabCtrl_InsertItem(hTab_, 0, &tie);
    tie.pszText = const_cast<LPWSTR>(L"  Business & Dial PINs  ");
    TabCtrl_InsertItem(hTab_, 1, &tie);
    tie.pszText = const_cast<LPWSTR>(L"  Universal Connection Guide  ");
    TabCtrl_InsertItem(hTab_, 2, &tie);

    // ----------------- TAB 0: Calls & Devices -----------------
    hList_ = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        35, 110, 960, 440, hWnd, TO_HMENU(IDC_DEVICE_LIST), hInst, NULL
    );
    SendMessage(hList_, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
    ListView_SetExtendedListViewStyle(hList_, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    // Add Columns to ListView
    struct ColDef { const wchar_t* title; int width; };
    ColDef cols[] = {
        { L"Device / Phone", 160 },
        { L"Dial PIN", 90 },
        { L"Platform", 100 },
        { L"Client IP", 120 },
        { L"Data Used", 95 },
        { L"Duration", 85 },
        { L"Billed Amount", 110 },
        { L"Status", 110 }
    };

    LVCOLUMNW lvc{};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    for (int i = 0; i < 8; ++i) {
        lvc.iSubItem = i;
        lvc.pszText = const_cast<LPWSTR>(cols[i].title);
        lvc.cx = cols[i].width;
        ListView_InsertColumn(hList_, i, &lvc);
    }

    btnApprove_ = CreateWindowExW(
        0, L"BUTTON", L"Approve / Activate",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        35, 565, 150, 36, hWnd, TO_HMENU(IDC_BTN_APPROVE), hInst, NULL
    );
    SendMessage(btnApprove_, WM_SETFONT, (WPARAM)hFontBold, TRUE);

    btnPause_ = CreateWindowExW(
        0, L"BUTTON", L"Pause Call",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        200, 565, 120, 36, hWnd, TO_HMENU(IDC_BTN_PAUSE), hInst, NULL
    );
    SendMessage(btnPause_, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

    btnHangUp_ = CreateWindowExW(
        0, L"BUTTON", L"Hang Up / Disconnect",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        335, 565, 170, 36, hWnd, TO_HMENU(IDC_BTN_HANGUP), hInst, NULL
    );
    SendMessage(btnHangUp_, WM_SETFONT, (WPARAM)hFontBold, TRUE);

    btnRefresh_ = CreateWindowExW(
        0, L"BUTTON", L"Refresh List",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        875, 565, 120, 36, hWnd, TO_HMENU(IDC_BTN_REFRESH), hInst, NULL
    );
    SendMessage(btnRefresh_, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

    // ----------------- TAB 1: Business & Passes -----------------
    // Group 1: Pricing Rates
    grpRates_ = CreateWindowExW(0, L"BUTTON", L"Monetization Rates (Your Internet Price)",
        WS_CHILD | BS_GROUPBOX, 35, 110, 460, 190, hWnd, NULL, hInst, NULL);
    SendMessage(grpRates_, WM_SETFONT, (WPARAM)hFontBold, TRUE);

    lblCurr_ = CreateWindowExW(0, L"STATIC", L"Currency (e.g. INR, USD):", WS_CHILD, 55, 140, 180, 20, hWnd, NULL, hInst, NULL);
    SendMessage(lblCurr_, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
    editCurr_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"INR", WS_CHILD | ES_AUTOHSCROLL, 240, 138, 120, 24, hWnd, TO_HMENU(IDC_EDIT_CURR), hInst, NULL);
    SendMessage(editCurr_, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

    lblRateGb_ = CreateWindowExW(0, L"STATIC", L"Rate per GB:", WS_CHILD, 55, 175, 180, 20, hWnd, NULL, hInst, NULL);
    SendMessage(lblRateGb_, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
    editRateGb_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"50.00", WS_CHILD | ES_AUTOHSCROLL, 240, 173, 120, 24, hWnd, TO_HMENU(IDC_EDIT_RATEGB), hInst, NULL);
    SendMessage(editRateGb_, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

    lblRateHr_ = CreateWindowExW(0, L"STATIC", L"Rate per Hour:", WS_CHILD, 55, 210, 180, 20, hWnd, NULL, hInst, NULL);
    SendMessage(lblRateHr_, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
    editRateHr_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"20.00", WS_CHILD | ES_AUTOHSCROLL, 240, 208, 120, 24, hWnd, TO_HMENU(IDC_EDIT_RATEHR), hInst, NULL);
    SendMessage(editRateHr_, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

    btnSaveRates_ = CreateWindowExW(0, L"BUTTON", L"Save Pricing Rates", WS_CHILD | BS_PUSHBUTTON, 240, 245, 150, 32, hWnd, TO_HMENU(IDC_BTN_SAVERATES), hInst, NULL);
    SendMessage(btnSaveRates_, WM_SETFONT, (WPARAM)hFontBold, TRUE);

    // Group 2: Dial PIN Pass Generator
    grpPassGen_ = CreateWindowExW(0, L"BUTTON", L"Generate Numeric Dial PIN Pass",
        WS_CHILD | BS_GROUPBOX, 520, 110, 475, 270, hWnd, NULL, hInst, NULL);
    SendMessage(grpPassGen_, WM_SETFONT, (WPARAM)hFontBold, TRUE);

    lblDur_ = CreateWindowExW(0, L"STATIC", L"Pass Duration:", WS_CHILD, 540, 140, 130, 20, hWnd, NULL, hInst, NULL);
    SendMessage(lblDur_, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
    cmbDur_ = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 680, 138, 180, 140, hWnd, TO_HMENU(IDC_CMB_DUR), hInst, NULL);
    SendMessage(cmbDur_, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
    SendMessageW(cmbDur_, CB_ADDSTRING, 0, (LPARAM)L"1 Hour");
    SendMessageW(cmbDur_, CB_ADDSTRING, 0, (LPARAM)L"3 Hours");
    SendMessageW(cmbDur_, CB_ADDSTRING, 0, (LPARAM)L"24 Hours (1 Day)");
    SendMessageW(cmbDur_, CB_ADDSTRING, 0, (LPARAM)L"7 Days");
    SendMessageW(cmbDur_, CB_ADDSTRING, 0, (LPARAM)L"Unlimited");
    SendMessageW(cmbDur_, CB_SETCURSEL, 2, 0); // 24 hours default

    lblQuota_ = CreateWindowExW(0, L"STATIC", L"Data Quota:", WS_CHILD, 540, 175, 130, 20, hWnd, NULL, hInst, NULL);
    SendMessage(lblQuota_, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
    cmbQuota_ = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 680, 173, 180, 140, hWnd, TO_HMENU(IDC_CMB_QUOTA), hInst, NULL);
    SendMessage(cmbQuota_, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
    SendMessageW(cmbQuota_, CB_ADDSTRING, 0, (LPARAM)L"500 MB");
    SendMessageW(cmbQuota_, CB_ADDSTRING, 0, (LPARAM)L"1 GB");
    SendMessageW(cmbQuota_, CB_ADDSTRING, 0, (LPARAM)L"2 GB");
    SendMessageW(cmbQuota_, CB_ADDSTRING, 0, (LPARAM)L"5 GB (Recommended)");
    SendMessageW(cmbQuota_, CB_ADDSTRING, 0, (LPARAM)L"10 GB");
    SendMessageW(cmbQuota_, CB_ADDSTRING, 0, (LPARAM)L"Unlimited");
    SendMessageW(cmbQuota_, CB_SETCURSEL, 3, 0); // 5 GB default

    btnGenPin_ = CreateWindowExW(0, L"BUTTON", L"Generate Dial PIN", WS_CHILD | BS_PUSHBUTTON, 680, 212, 180, 34, hWnd, TO_HMENU(IDC_BTN_GENPIN), hInst, NULL);
    SendMessage(btnGenPin_, WM_SETFONT, (WPARAM)hFontBold, TRUE);

    editGenPin_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"Click 'Generate Dial PIN' above", WS_CHILD | ES_READONLY | ES_CENTER, 540, 258, 430, 32, hWnd, TO_HMENU(IDC_EDIT_GENPIN), hInst, NULL);
    SendMessage(editGenPin_, WM_SETFONT, (WPARAM)GetDefaultSegoeFont(18, FW_BOLD), TRUE);

    btnCopyMsg_ = CreateWindowExW(0, L"BUTTON", L"Copy Shareable WhatsApp / SMS Pass Message", WS_CHILD | BS_PUSHBUTTON, 540, 302, 430, 36, hWnd, TO_HMENU(IDC_BTN_COPYMSG), hInst, NULL);
    SendMessage(btnCopyMsg_, WM_SETFONT, (WPARAM)hFontBold, TRUE);

    // Group 3: Lifetime Earnings Stats
    grpStats_ = CreateWindowExW(0, L"BUTTON", L"Lifetime Earnings Ledger",
        WS_CHILD | BS_GROUPBOX, 35, 320, 460, 240, hWnd, NULL, hInst, NULL);
    SendMessage(grpStats_, WM_SETFONT, (WPARAM)hFontBold, TRUE);

    lblLifetimeRev_ = CreateWindowExW(0, L"STATIC", L"Total Revenue Earned: \xE2\x82\xB9 0.00", WS_CHILD, 55, 360, 420, 26, hWnd, NULL, hInst, NULL);
    SendMessage(lblLifetimeRev_, WM_SETFONT, (WPARAM)GetDefaultSegoeFont(18, FW_BOLD), TRUE);

    lblLifetimeGb_ = CreateWindowExW(0, L"STATIC", L"Total Internet Sold: 0.00 GB", WS_CHILD, 55, 400, 420, 24, hWnd, NULL, hInst, NULL);
    SendMessage(lblLifetimeGb_, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

    lblTotalCalls_ = CreateWindowExW(0, L"STATIC", L"Total Call Sessions: 0", WS_CHILD, 55, 430, 420, 24, hWnd, NULL, hInst, NULL);
    SendMessage(lblTotalCalls_, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

    // ----------------- TAB 2: Universal Guide -----------------
    editGuide_ = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_HSCROLL,
        35, 110, 960, 480, hWnd, TO_HMENU(IDC_EDIT_GUIDE), hInst, NULL
    );
    SendMessage(editGuide_, WM_SETFONT, (WPARAM)GetDefaultSegoeFont(14, FW_NORMAL), TRUE);

    SwitchTab(0);
}

void MainWindow::SwitchTab(int tab_index) {
    current_tab_ = tab_index;

    // Tab 0 controls
    int showTab0 = (tab_index == 0) ? SW_SHOW : SW_HIDE;
    ShowWindow(hList_, showTab0);
    ShowWindow(btnApprove_, showTab0);
    ShowWindow(btnPause_, showTab0);
    ShowWindow(btnHangUp_, showTab0);
    ShowWindow(btnRefresh_, showTab0);

    // Tab 1 controls
    int showTab1 = (tab_index == 1) ? SW_SHOW : SW_HIDE;
    ShowWindow(grpRates_, showTab1);
    ShowWindow(lblCurr_, showTab1);
    ShowWindow(editCurr_, showTab1);
    ShowWindow(lblRateGb_, showTab1);
    ShowWindow(editRateGb_, showTab1);
    ShowWindow(lblRateHr_, showTab1);
    ShowWindow(editRateHr_, showTab1);
    ShowWindow(btnSaveRates_, showTab1);

    ShowWindow(grpPassGen_, showTab1);
    ShowWindow(lblDur_, showTab1);
    ShowWindow(cmbDur_, showTab1);
    ShowWindow(lblQuota_, showTab1);
    ShowWindow(cmbQuota_, showTab1);
    ShowWindow(btnGenPin_, showTab1);
    ShowWindow(editGenPin_, showTab1);
    ShowWindow(btnCopyMsg_, showTab1);

    ShowWindow(grpStats_, showTab1);
    ShowWindow(lblLifetimeRev_, showTab1);
    ShowWindow(lblLifetimeGb_, showTab1);
    ShowWindow(lblTotalCalls_, showTab1);

    // Tab 2 controls
    int showTab2 = (tab_index == 2) ? SW_SHOW : SW_HIDE;
    ShowWindow(editGuide_, showTab2);

    if (tab_index == 0) {
        RefreshDeviceList();
    } else if (tab_index == 1) {
        RefreshBusinessStats();
    } else if (tab_index == 2) {
        // Populate Universal Connection Guide
        std::string pub_ip = !public_ip_cached_.empty() ? public_ip_cached_ : "127.0.0.1";
        std::ostringstream ss;
        ss << "========================================================================================\r\n"
           << "  OPENROUTE UNIVERSAL CONNECTION GUIDE (CONNECT ANY PHONE, LAPTOP, OR OS)\r\n"
           << "========================================================================================\r\n\r\n"
           << "Your OpenRoute Gateway shares your Wi-Fi or 5G internet connection with remote users.\r\n"
           << "Remote users can be located anywhere (different house, city, or on cellular data).\r\n\r\n"
           << "Your Host Addresses:\r\n"
           << "  * Remote / Public Endpoint: " << pub_ip << ":8080\r\n"
           << "  * Local Wi-Fi Endpoint:     192.168.0.239:8080 (or your local PC IP)\r\n"
           << "  * Universal Proxy Port:     8080\r\n"
           << "  * WireGuard VPN Port:       51820\r\n\r\n"
           << "----------------------------------------------------------------------------------------\r\n"
           << "METHOD 1: ZERO APPS NEEDED (BUILT INTO EVERY PHONE & OS)\r\n"
           << "----------------------------------------------------------------------------------------\r\n"
           << "Any client can route their internet through your PC without installing any app:\r\n\r\n"
           << "1. APPLE IPHONE / IPAD (iOS):\r\n"
           << "   - Open Settings -> Wi-Fi -> Tap the (i) icon next to connected network.\r\n"
           << "   - Scroll down to 'Configure Proxy' -> Select 'Manual'.\r\n"
           << "   - Server: " << pub_ip << "   |   Port: 8080\r\n"
           << "   - Turn 'Authentication' ON -> Enter username: openroute, Password: <Customer Dial PIN>.\r\n"
           << "   - Tap Save. Their entire phone now routes internet through your host PC!\r\n\r\n"
           << "2. ANDROID PHONES:\r\n"
           << "   - Open Settings -> Network / Wi-Fi -> Tap your Wi-Fi name -> Advanced Options.\r\n"
           << "   - Select Proxy -> Manual -> Proxy Hostname: " << pub_ip << ", Proxy Port: 8080.\r\n\r\n"
           << "3. WINDOWS / MAC LAPTOPS:\r\n"
           << "   - Windows: Settings -> Network & Internet -> Proxy -> Set Proxy: " << pub_ip << ":8080.\r\n"
           << "   - Mac: System Settings -> Network -> Proxies -> Web Proxy (HTTP/HTTPS): " << pub_ip << ":8080.\r\n\r\n"
           << "----------------------------------------------------------------------------------------\r\n"
           << "METHOD 2: PHONE DIAL-PAD PORTAL (DIAL TO CONNECT - HANG UP TO STOP)\r\n"
           << "----------------------------------------------------------------------------------------\r\n"
           << "1. Remote user opens their mobile web browser to: http://" << pub_ip << ":8080/dial\r\n"
           << "2. A realistic phone keypad appears on their phone screen.\r\n"
           << "3. They enter their 6-digit Dial PIN (e.g. 849-201) and tap the green DIAL button.\r\n"
           << "4. Internet session is immediately activated!\r\n"
           << "5. Live data meter displays their MB used and call duration.\r\n"
           << "6. When they tap 'HANG UP', the call ends and internet stops immediately.\r\n\r\n"
           << "----------------------------------------------------------------------------------------\r\n"
           << "REMOTE ACCESS NOTES (FOR USERS FAR AWAY):\r\n"
           << "----------------------------------------------------------------------------------------\r\n"
           << "- Ensure port 8080 (TCP) and 51820 (UDP) are forwarded on your home Wi-Fi router to this PC.\r\n"
           << "- If behind ISP CGNAT, you can run a free tunnel (e.g. Cloudflare Tunnel, ngrok, or playit.gg)\r\n"
           << "  and give remote users your public tunnel address.\r\n";
        SetWindowTextA_UTF8(editGuide_, ss.str());
    }
}

void MainWindow::UpdateHeaderStatus() {
    auto& billing = gateway_.billing_manager();
    auto summary = billing.GetSummary();

    // Check STUN prober or public IP
    if (public_ip_cached_.empty()) {
        try {
            networking::NatProbe probe;
            auto res = probe.PerformFullProbe();
            if (!res.public_ip.empty()) {
                public_ip_cached_ = res.public_ip;
                if (gateway_.proxy()) {
                    gateway_.proxy()->SetPublicHost(res.public_ip);
                }
            }
        } catch (...) {}
    }

    std::string ip_show = !public_ip_cached_.empty() ? public_ip_cached_ : "127.0.0.1";
    std::ostringstream ss;
    ss << "OPENROUTE GATEWAY   [● LIVE - SHARING 5G/WI-FI]   |   Public: "
       << ip_show << ":8080   |   Active Calls: " << summary.active_sessions
       << "   |   Revenue: " << accounting::BillingManager::FormatCurrency(summary.total_revenue, summary.currency);

    SetWindowTextA_UTF8(hHeader_, ss.str());
}

void MainWindow::RefreshDeviceList() {
    auto& billing = gateway_.billing_manager();
    auto calls = billing.GetAllCalls();
    int currentCount = ListView_GetItemCount(hList_);

    if (currentCount == static_cast<int>(calls.size())) {
        // In-place update: never deletes rows, never deselects the user's clicked item!
        for (int i = 0; i < static_cast<int>(calls.size()); ++i) {
            const auto& c = calls[i];

            std::wstring wName = ToWString(c.device_name);
            ListView_SetItemText(hList_, i, 0, const_cast<LPWSTR>(wName.c_str()));

            std::wstring wPin = ToWString(c.dial_pin);
            ListView_SetItemText(hList_, i, 1, const_cast<LPWSTR>(wPin.c_str()));

            std::wstring wPlat = ToWString(c.platform);
            ListView_SetItemText(hList_, i, 2, const_cast<LPWSTR>(wPlat.c_str()));

            std::wstring wIp = ToWString(c.client_ip.empty() ? "Pending Connect" : c.client_ip);
            ListView_SetItemText(hList_, i, 3, const_cast<LPWSTR>(wIp.c_str()));

            std::string used = accounting::BillingManager::FormatBytes(c.bytes_rx + c.bytes_tx);
            std::wstring wUsed = ToWString(used);
            ListView_SetItemText(hList_, i, 4, const_cast<LPWSTR>(wUsed.c_str()));

            int64_t now_epoch = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            int64_t dur = (c.end_time > 0 ? c.end_time : now_epoch) - c.start_time;
            std::string durStr = accounting::BillingManager::FormatDuration(dur);
            std::wstring wDur = ToWString(durStr);
            ListView_SetItemText(hList_, i, 5, const_cast<LPWSTR>(wDur.c_str()));

            std::string billStr = accounting::BillingManager::FormatCurrency(c.billed_amount, billing.GetRates().currency);
            std::wstring wBill = ToWString(billStr);
            ListView_SetItemText(hList_, i, 6, const_cast<LPWSTR>(wBill.c_str()));

            std::wstring wStatus = L"ACTIVE";
            if (c.is_hung_up) wStatus = L"HUNG UP";
            else if (c.is_paused) wStatus = L"PAUSED";
            else if (!c.is_approved) wStatus = L"PENDING";
            ListView_SetItemText(hList_, i, 7, const_cast<LPWSTR>(wStatus.c_str()));
        }
        return;
    }

    // Row count changed: full rebuild, then restore selection state
    ListView_DeleteAllItems(hList_);
    LVITEMW lvi{};
    lvi.mask = LVIF_TEXT;

    int reselectIdx = -1;
    for (int i = 0; i < static_cast<int>(calls.size()); ++i) {
        const auto& c = calls[i];
        lvi.iItem = i;
        lvi.iSubItem = 0;

        std::wstring wName = ToWString(c.device_name);
        lvi.pszText = const_cast<LPWSTR>(wName.c_str());
        ListView_InsertItem(hList_, &lvi);

        std::wstring wPin = ToWString(c.dial_pin);
        ListView_SetItemText(hList_, i, 1, const_cast<LPWSTR>(wPin.c_str()));
        if (!selected_pin_.empty() && c.dial_pin == selected_pin_) {
            reselectIdx = i;
        }

        std::wstring wPlat = ToWString(c.platform);
        ListView_SetItemText(hList_, i, 2, const_cast<LPWSTR>(wPlat.c_str()));

        std::wstring wIp = ToWString(c.client_ip.empty() ? "Pending Connect" : c.client_ip);
        ListView_SetItemText(hList_, i, 3, const_cast<LPWSTR>(wIp.c_str()));

        std::string used = accounting::BillingManager::FormatBytes(c.bytes_rx + c.bytes_tx);
        std::wstring wUsed = ToWString(used);
        ListView_SetItemText(hList_, i, 4, const_cast<LPWSTR>(wUsed.c_str()));

        int64_t now_epoch = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        int64_t dur = (c.end_time > 0 ? c.end_time : now_epoch) - c.start_time;
        std::string durStr = accounting::BillingManager::FormatDuration(dur);
        std::wstring wDur = ToWString(durStr);
        ListView_SetItemText(hList_, i, 5, const_cast<LPWSTR>(wDur.c_str()));

        std::string billStr = accounting::BillingManager::FormatCurrency(c.billed_amount, billing.GetRates().currency);
        std::wstring wBill = ToWString(billStr);
        ListView_SetItemText(hList_, i, 6, const_cast<LPWSTR>(wBill.c_str()));

        std::wstring wStatus = L"ACTIVE";
        if (c.is_hung_up) wStatus = L"HUNG UP";
        else if (c.is_paused) wStatus = L"PAUSED";
        else if (!c.is_approved) wStatus = L"PENDING";
        ListView_SetItemText(hList_, i, 7, const_cast<LPWSTR>(wStatus.c_str()));
    }

    if (reselectIdx >= 0) {
        ListView_SetItemState(hList_, reselectIdx, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    }
}

void MainWindow::RefreshBusinessStats() {
    auto& billing = gateway_.billing_manager();
    auto rates = billing.GetRates();
    auto summary = billing.GetSummary();

    SetWindowTextA_UTF8(editCurr_, rates.currency);

    std::ostringstream ssGb;
    ssGb << std::fixed << std::setprecision(2) << rates.rate_per_gb;
    SetWindowTextA_UTF8(editRateGb_, ssGb.str());

    std::ostringstream ssHr;
    ssHr << std::fixed << std::setprecision(2) << rates.rate_per_hour;
    SetWindowTextA_UTF8(editRateHr_, ssHr.str());

    std::string rev = "Total Revenue Earned: " + accounting::BillingManager::FormatCurrency(summary.total_revenue, summary.currency);
    SetWindowTextA_UTF8(lblLifetimeRev_, rev);

    std::string gbSold = "Total Internet Sold: " + accounting::BillingManager::FormatBytes(summary.total_bytes);
    SetWindowTextA_UTF8(lblLifetimeGb_, gbSold);

    std::string sess = "Total Call Sessions: " + std::to_string(summary.total_sessions) + " (" + std::to_string(summary.active_sessions) + " Active)";
    SetWindowTextA_UTF8(lblTotalCalls_, sess);
}

void MainWindow::OnApproveClicked() {
    std::string pin = selected_pin_;
    if (pin.empty()) {
        int sel = ListView_GetNextItem(hList_, -1, LVNI_SELECTED);
        if (sel >= 0) {
            wchar_t pinBuf[64]{};
            ListView_GetItemText(hList_, sel, 1, pinBuf, 64);
            pin = ToString(pinBuf);
        }
    }

    if (pin.empty()) {
        MessageBoxW(hWnd_, L"Please click a device / call row to select it.", L"OpenRoute", MB_OK | MB_ICONINFORMATION);
        return;
    }

    auto call = gateway_.billing_manager().FindCallByPin(pin);
    if (call) {
        gateway_.billing_manager().ApproveCall(call->session_id);
        RefreshDeviceList();
        RefreshBusinessStats();
        MessageBoxW(hWnd_, L"Session approved! Remote device now has active internet.", L"OpenRoute", MB_OK | MB_ICONINFORMATION);
    }
}

void MainWindow::OnPauseClicked() {
    std::string pin = selected_pin_;
    if (pin.empty()) {
        int sel = ListView_GetNextItem(hList_, -1, LVNI_SELECTED);
        if (sel >= 0) {
            wchar_t pinBuf[64]{};
            ListView_GetItemText(hList_, sel, 1, pinBuf, 64);
            pin = ToString(pinBuf);
        }
    }

    if (pin.empty()) {
        MessageBoxW(hWnd_, L"Please click a device / call row to select it.", L"OpenRoute", MB_OK | MB_ICONINFORMATION);
        return;
    }

    auto call = gateway_.billing_manager().FindCallByPin(pin);
    if (call) {
        bool newPause = !call->is_paused;
        gateway_.billing_manager().PauseCall(call->session_id, newPause);
        RefreshDeviceList();
        RefreshBusinessStats();
    }
}

void MainWindow::OnHangUpClicked() {
    std::string pin = selected_pin_;
    if (pin.empty()) {
        int sel = ListView_GetNextItem(hList_, -1, LVNI_SELECTED);
        if (sel >= 0) {
            wchar_t pinBuf[64]{};
            ListView_GetItemText(hList_, sel, 1, pinBuf, 64);
            pin = ToString(pinBuf);
        }
    }

    if (pin.empty()) {
        MessageBoxW(hWnd_, L"Please click a device / call row to select it.", L"OpenRoute", MB_OK | MB_ICONINFORMATION);
        return;
    }

    auto call = gateway_.billing_manager().FindCallByPin(pin);
    if (call) {
        gateway_.billing_manager().HangUpCall(call->session_id);
        selected_pin_.clear();
        RefreshDeviceList();
        RefreshBusinessStats();
        MessageBoxW(hWnd_, L"Call hung up! Internet connection terminated and usage finalized.", L"OpenRoute", MB_OK | MB_ICONINFORMATION);
    }
}

void MainWindow::OnSaveRatesClicked() {
    wchar_t currBuf[32]{}, gbBuf[32]{}, hrBuf[32]{};
    GetWindowTextW(editCurr_, currBuf, 32);
    GetWindowTextW(editRateGb_, gbBuf, 32);
    GetWindowTextW(editRateHr_, hrBuf, 32);

    accounting::PricingRates r;
    r.currency = ToString(currBuf);
    try {
        r.rate_per_gb = std::stod(ToString(gbBuf));
        r.rate_per_hour = std::stod(ToString(hrBuf));
    } catch (...) {
        MessageBoxW(hWnd_, L"Invalid number format in pricing fields.", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    gateway_.billing_manager().UpdateRates(r);
    RefreshBusinessStats();
    UpdateHeaderStatus();
    MessageBoxW(hWnd_, L"Pricing rates saved successfully!", L"OpenRoute", MB_OK | MB_ICONINFORMATION);
}

void MainWindow::OnGeneratePinClicked() {
    int durSel = static_cast<int>(SendMessageW(cmbDur_, CB_GETCURSEL, 0, 0));
    int quotaSel = static_cast<int>(SendMessageW(cmbQuota_, CB_GETCURSEL, 0, 0));

    int64_t durSec = 86400; // 24h default
    if (durSel == 0) durSec = 3600;
    else if (durSel == 1) durSec = 10800;
    else if (durSel == 2) durSec = 86400;
    else if (durSel == 3) durSec = 604800;
    else if (durSel == 4) durSec = 0; // unlimited

    uint64_t quotaBytes = 5ULL * 1024 * 1024 * 1024; // 5GB default
    if (quotaSel == 0) quotaBytes = 500ULL * 1024 * 1024;
    else if (quotaSel == 1) quotaBytes = 1ULL * 1024 * 1024 * 1024;
    else if (quotaSel == 2) quotaBytes = 2ULL * 1024 * 1024 * 1024;
    else if (quotaSel == 3) quotaBytes = 5ULL * 1024 * 1024 * 1024;
    else if (quotaSel == 4) quotaBytes = 10ULL * 1024 * 1024 * 1024;
    else if (quotaSel == 5) quotaBytes = 0; // unlimited

    std::string pin = gateway_.proxy() ? gateway_.proxy()->GenerateDialPin(6) : "849201";
    latest_generated_pin_ = pin;

    if (gateway_.proxy()) {
        gateway_.proxy()->CreateDialPass(pin, quotaBytes, durSec, "Universal");
    }

    std::string disp = "DIAL PIN: " + pin.substr(0, 3) + "-" + pin.substr(3);
    SetWindowTextA_UTF8(editGenPin_, disp);

    RefreshDeviceList();
    RefreshBusinessStats();
}

void MainWindow::OnCopyCustomerMessageClicked() {
    if (latest_generated_pin_.empty()) {
        OnGeneratePinClicked();
    }

    std::string pub_ip = !public_ip_cached_.empty() ? public_ip_cached_ : "127.0.0.1";
    std::string pin = latest_generated_pin_;

    std::ostringstream ss;
    ss << "=== OPENROUTE 5G/WI-FI INTERNET PASS ===\r\n"
       << "Your Internet Dial PIN: " << pin << "\r\n"
       << "Host Address: " << pub_ip << ":8080\r\n\r\n"
       << "HOW TO CONNECT FROM YOUR PHONE / LAPTOP:\r\n"
       << "Method A (Zero Apps Required - iPhone / Android / PC):\r\n"
       << "1. Go to your phone's Wi-Fi Settings -> Configure Proxy -> Manual.\r\n"
       << "2. Server: " << pub_ip << " | Port: 8080\r\n"
       << "3. Enter PIN as password: " << pin << "\r\n\r\n"
       << "Method B (Phone Dial-Pad Web):\r\n"
       << "Open in browser: http://" << pub_ip << ":8080/dial?pin=" << pin << "\r\n"
       << "Tap 'DIAL' to activate internet. Tap 'HANG UP' when finished.\r\n";

    std::wstring wmsg = ToWString(ss.str());

    if (OpenClipboard(hWnd_)) {
        EmptyClipboard();
        HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, (wmsg.size() + 1) * sizeof(wchar_t));
        if (hGlob) {
            wchar_t* pGlob = static_cast<wchar_t*>(GlobalLock(hGlob));
            if (pGlob) {
                wcscpy_s(pGlob, wmsg.size() + 1, wmsg.c_str());
                GlobalUnlock(hGlob);
                SetClipboardData(CF_UNICODETEXT, hGlob);
            }
        }
        CloseClipboard();
        MessageBoxW(hWnd_, L"Pass instructions copied to clipboard! You can now paste and send to customer via WhatsApp or SMS.", L"Copied", MB_OK | MB_ICONINFORMATION);
    }
}

} // namespace openroute::gui
