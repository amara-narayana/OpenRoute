#include "core/tunnel/wintun_adapter.hpp"
#include "core/common/logger.hpp"

#include <cstring>

namespace openroute::tunnel {

WintunAdapter::WintunAdapter() = default;

WintunAdapter::~WintunAdapter() {
    Close();
}

bool WintunAdapter::LoadDriver(const std::wstring& dll_path) {
#if defined(_WIN32)
    std::lock_guard<std::mutex> lock(mutex_);
    if (wintun_module_) return true;

    wintun_module_ = LoadLibraryW(dll_path.c_str());
    if (!wintun_module_) {
        // Fallback to relative path in third_party
        wintun_module_ = LoadLibraryW(L"third_party/wintun/bin/amd64/wintun.dll");
    }

    if (!wintun_module_) {
        LOG_WARN << "Failed to load wintun.dll (Error: " << GetLastError() << ").";
        return false;
    }

#define RESOLVE_FUNC(name, type) \
    fn_##name##_ = reinterpret_cast<type*>(GetProcAddress(wintun_module_, "Wintun" #name)); \
    if (!fn_##name##_) { \
        LOG_ERROR << "Failed to resolve Wintun" #name " function from wintun.dll"; \
        FreeLibrary(wintun_module_); \
        wintun_module_ = nullptr; \
        return false; \
    }

    RESOLVE_FUNC(CreateAdapter, WINTUN_CREATE_ADAPTER_FUNC)
    RESOLVE_FUNC(OpenAdapter, WINTUN_OPEN_ADAPTER_FUNC)
    RESOLVE_FUNC(CloseAdapter, WINTUN_CLOSE_ADAPTER_FUNC)
    RESOLVE_FUNC(StartSession, WINTUN_START_SESSION_FUNC)
    RESOLVE_FUNC(EndSession, WINTUN_END_SESSION_FUNC)
    RESOLVE_FUNC(GetReadWaitEvent, WINTUN_GET_READ_WAIT_EVENT_FUNC)
    RESOLVE_FUNC(ReceivePacket, WINTUN_RECEIVE_PACKET_FUNC)
    RESOLVE_FUNC(ReleaseReceivePacket, WINTUN_RELEASE_RECEIVE_PACKET_FUNC)
    RESOLVE_FUNC(AllocateSendPacket, WINTUN_ALLOCATE_SEND_PACKET_FUNC)
    RESOLVE_FUNC(SendPacket, WINTUN_SEND_PACKET_FUNC)

#undef RESOLVE_FUNC

    LOG_INFO << "wintun.dll loaded and symbols resolved successfully.";
    return true;
#else
    return false;
#endif
}

bool WintunAdapter::OpenOrCreate(const std::wstring& adapter_name, const std::wstring& tunnel_type) {
#if defined(_WIN32)
    std::lock_guard<std::mutex> lock(mutex_);
    if (!wintun_module_) {
        if (!LoadDriver()) {
            return false;
        }
    }

    // Attempt to open existing adapter
    adapter_handle_ = fn_OpenAdapter_(adapter_name.c_str());
    if (!adapter_handle_) {
        // Create new adapter
        adapter_handle_ = fn_CreateAdapter_(adapter_name.c_str(), tunnel_type.c_str(), nullptr);
    }

    if (!adapter_handle_) {
        LOG_ERROR << "Failed to create or open Wintun adapter (Error: " << GetLastError() << "). Requires administrator privileges.";
        return false;
    }

    LOG_INFO << "Wintun adapter handle opened successfully.";
    return true;
#else
    (void)adapter_name; (void)tunnel_type;
    return false;
#endif
}

bool WintunAdapter::StartSession(uint32_t ring_capacity) {
#if defined(_WIN32)
    std::lock_guard<std::mutex> lock(mutex_);
    if (!adapter_handle_ || !fn_StartSession_) {
        return false;
    }

    session_handle_ = fn_StartSession_(adapter_handle_, ring_capacity);
    if (!session_handle_) {
        LOG_ERROR << "Failed to start Wintun session (Error: " << GetLastError() << ")";
        return false;
    }

    session_active_ = true;
    LOG_INFO << "Wintun session started with ring capacity: " << ring_capacity << " bytes";
    return true;
#else
    (void)ring_capacity;
    return false;
#endif
}

void WintunAdapter::Close() {
#if defined(_WIN32)
    std::lock_guard<std::mutex> lock(mutex_);
    if (session_handle_ && fn_EndSession_) {
        fn_EndSession_(session_handle_);
        session_handle_ = nullptr;
    }
    session_active_ = false;

    if (adapter_handle_ && fn_CloseAdapter_) {
        fn_CloseAdapter_(adapter_handle_);
        adapter_handle_ = nullptr;
    }

    if (wintun_module_) {
        FreeLibrary(wintun_module_);
        wintun_module_ = nullptr;
    }
#endif
}

bool WintunAdapter::ReadPacket(std::vector<uint8_t>& out_packet) {
#if defined(_WIN32)
    if (!session_handle_ || !fn_ReceivePacket_ || !fn_ReleaseReceivePacket_) {
        return false;
    }

    DWORD packet_size = 0;
    BYTE* packet_data = fn_ReceivePacket_(session_handle_, &packet_size);
    if (!packet_data) {
        return false;
    }

    out_packet.assign(packet_data, packet_data + packet_size);
    fn_ReleaseReceivePacket_(session_handle_, packet_data);
    return true;
#else
    (void)out_packet;
    return false;
#endif
}

bool WintunAdapter::WritePacket(const uint8_t* data, size_t len) {
#if defined(_WIN32)
    if (!session_handle_ || !fn_AllocateSendPacket_ || !fn_SendPacket_) {
        return false;
    }

    if (len > WINTUN_MAX_IP_PACKET_SIZE) {
        return false;
    }

    BYTE* send_buffer = fn_AllocateSendPacket_(session_handle_, static_cast<DWORD>(len));
    if (!send_buffer) {
        return false;
    }

    std::memcpy(send_buffer, data, len);
    fn_SendPacket_(session_handle_, send_buffer);
    return true;
#else
    (void)data; (void)len;
    return false;
#endif
}

#if defined(_WIN32)
HANDLE WintunAdapter::GetReadWaitEvent() const {
    if (!session_handle_ || !fn_GetReadWaitEvent_) {
        return nullptr;
    }
    return fn_GetReadWaitEvent_(session_handle_);
}
#endif

bool WintunAdapter::IsActive() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return session_active_;
}

} // namespace openroute::tunnel
