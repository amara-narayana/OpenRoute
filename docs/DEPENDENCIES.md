# OpenRoute — Dependencies & Licensing Evaluation

## 1. Third-Party Dependency Manifest

| Dependency | Purpose | Version | License | Justification & Distribution Impact |
| :--- | :--- | :--- | :--- | :--- |
| **Wintun** | Windows Virtual TUN Network Adapter driver | `0.14.1` | **MIT License** | High performance, created by Jason Donenfeld (WireGuard project). Microsoft WHQL signed, runs natively on Windows 10/11 64-bit. Permissive MIT license allows shipping `wintun.dll` without GPL copyleft contagions. |
| **Monocypher** | Cryptographic primitives (X25519, ChaCha20-Poly1305, BLAKE2s) | `4.0.2` | **2-Clause BSD / CC0-1.0** | Audited, constant-time, zero external dependencies, ultra-lightweight (two files: `.h` and `.c`). Directly provides exact cryptographic primitives required for WireGuard protocol (Noise_IKpsk2) and token hashing. |
| **SQLite3 (Amalgamation)** | Local gateway state, metadata, devices, quotas, audit log | `3.45+` | **Public Domain / CC0** | Fast, zero server process, robust ACID transactions, WAL mode. Embedded directly into C++ core. |
| **nlohmann/json** | Structured configuration, IPC, JSON-RPC, invitation tokens | `3.11.3` | **MIT License** | Standard single-header JSON library in modern C++. Clean, type-safe serialization. |
| **GoogleTest** | Unit and integration testing | `1.14.0` | **BSD 3-Clause** | Industry standard C++ testing framework. Minimal compile footprint, excellent assertion reporting. |
| **Android WireGuard Tunnel / VpnService** | Android tunnel backend (`com.wireguard.android:tunnel`) | `1.0.2023xxxx` | **Apache 2.0** | Official maintained Android library for WireGuard tunnel handling over `android.net.VpnService`. |

---

## 2. WireGuard Implementation Evaluation for Windows

We investigated the following three primary architectural options for Windows:

### Option A: Kernel-Space WireGuardNT (`wireguard-nt` / `wireguard.sys`)
- **Pros**: Runs in Windows kernel; near-zero context switches.
- **Cons**:
  1. **License Contagion**: WireGuardNT is licensed strictly under **GNU General Public License v2 (GPLv2)**. Linking proprietary components or closed commercial extensions directly creates GPL copyleft complications.
  2. **Driver Signing & Installation**: Installing kernel drivers requires EV Authenticode code-signing certificates and kernel-mode driver installation privileges. Self-signed or unsigned drivers cannot load on retail Windows 11 with Secure Boot enabled.
  3. **Lack of Dynamic Policy Injection**: Kernel WireGuardNT forwards packets entirely in kernel space without exposing per-packet userspace hooks for our token-bucket rate limiter, granular quota tracking, or custom dynamic packet-level accounting before packet drops.

### Option B: Wintun + Userspace WireGuard Engine (Selected Architecture)
- **Pros**:
  1. **MIT Licensed**: Jason Donenfeld's `wintun.dll` and Microsoft WHQL-signed `wintun.sys` are permissively licensed under MIT.
  2. **WHQL Signed**: Installs and loads seamlessly on standard retail Windows 11 64-bit without disabling Secure Boot or driver enforcement.
  3. **Full Packet Control**: The userspace engine reads raw IP packets from the Wintun ring buffer and encrypts them onto UDP sockets. This gives OpenRoute 100% granular control to:
     - Enforce per-device token-bucket rate limiting (bandwidth caps).
     - Maintain atomic byte counters for daily/monthly quotas.
     - Isolate LAN traffic (drop packets targeting `192.168.0.0/16`, `10.0.0.0/8`, `172.16.0.0/12`).
     - Immediately terminate peer sessions on revocation without restarting the driver.
  4. **Performance**: Wintun uses memory-mapped circular ring buffers between kernel and userspace, capable of gigabit throughput with low CPU overhead.
  5. **Standard Interoperability**: Implements the official WireGuard Noise_IKpsk2 protocol specification. Official WireGuard clients on Android and Windows can interoperate out-of-the-box.
- **Cons**: Minor userspace-kernel transition overhead compared to pure kernel, but negligible on modern multi-core x86_64 CPUs for consumer/remote-worker sharing scenarios.

**Decision**: **Adopt Option B (Wintun + Native C++20 WireGuard Protocol Engine with Monocypher)**.

---

## 3. Cryptographic Primitives & Protocol Standards

OpenRoute does **not** invent proprietary cryptography:
1. **Key Exchange**: X25519 (ECDH over Curve25519, RFC 7748).
2. **Authenticated Encryption**: ChaCha20-Poly1305 (RFC 8439), 128-bit authentication tag, 64-bit counter nonce.
3. **Hashing & Key Derivation**: BLAKE2s-256 (RFC 7693) and HKDF-BLAKE2s.
4. **Handshake**: Noise_IKpsk2 (initiator static pre-known to responder; responder static pre-known to initiator; optional preshared symmetric key).
5. **Secure Local Storage**: Windows Data Protection API (`CryptProtectData` / DPAPI) for protecting private keys on disk, Android Keystore on Android.
