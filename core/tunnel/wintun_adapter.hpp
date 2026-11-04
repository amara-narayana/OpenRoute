#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "third_party/wintun/wintun.h"
#endif

namespace openroute::tunnel {

class WintunAdapter {
public:
    WintunAdapter();
    ~WintunAdapter();

    WintunAdapter(const WintunAdapter&) = delete;
    WintunAdapter& operator=(const WintunAdapter&) = delete;

    bool LoadDriver(const std::wstring& dll_path = L"wintun.dll");
    bool OpenOrCreate(const std::wstring& adapter_name = L"openroute0",
                      const std::wstring& tunnel_type = L"OpenRoute");
    bool StartSession(uint32_t ring_capacity = 0x400000); // 4 MB default ring capacity
    void Close();

    bool ReadPacket(std::vector<uint8_t>& out_packet);
    bool WritePacket(const uint8_t* data, size_t len);

#if defined(_WIN32)
    HANDLE GetReadWaitEvent() const;
#endif

    [[nodiscard]] bool IsLoaded() const { return wintun_module_ != nullptr; }
    [[nodiscard]] bool IsActive() const;

private:
#if defined(_WIN32)
    HMODULE wintun_module_ = nullptr;
    WINTUN_ADAPTER_HANDLE adapter_handle_ = nullptr;
    WINTUN_SESSION_HANDLE session_handle_ = nullptr;

    // Function pointers resolved from wintun.dll
    WINTUN_CREATE_ADAPTER_FUNC* fn_CreateAdapter_ = nullptr;
    WINTUN_OPEN_ADAPTER_FUNC* fn_OpenAdapter_ = nullptr;
    WINTUN_CLOSE_ADAPTER_FUNC* fn_CloseAdapter_ = nullptr;
    WINTUN_START_SESSION_FUNC* fn_StartSession_ = nullptr;
    WINTUN_END_SESSION_FUNC* fn_EndSession_ = nullptr;
    WINTUN_GET_READ_WAIT_EVENT_FUNC* fn_GetReadWaitEvent_ = nullptr;
    WINTUN_RECEIVE_PACKET_FUNC* fn_ReceivePacket_ = nullptr;
    WINTUN_RELEASE_RECEIVE_PACKET_FUNC* fn_ReleaseReceivePacket_ = nullptr;
    WINTUN_ALLOCATE_SEND_PACKET_FUNC* fn_AllocateSendPacket_ = nullptr;
    WINTUN_SEND_PACKET_FUNC* fn_SendPacket_ = nullptr;
#else
    void* wintun_module_ = nullptr;
#endif

    mutable std::mutex mutex_;
    bool session_active_ = false;
};

} // namespace openroute::tunnel
