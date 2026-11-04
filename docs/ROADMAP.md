# OpenRoute — Implementation Roadmap & Milestones

This document defines the strict, phased delivery plan for OpenRoute. Development proceeds milestone by milestone. Features are marked complete only after implementation and empirical verification.

---

## Phase 0: Environment Inspection & Dependencies
- [x] Inspect Windows version, CPU, RAM, disk space, network adapters.
- [x] Audit available compilers (MSVC 2022 v19.44, GCC 16.2), CMake (4.4.0), Ninja (1.13.2), Python (3.13.0), Git (2.55.0).
- [x] Audit Android SDK (`android-35`, `android-36`) and NDK (`26.3`, `29.0`).
- [x] Evaluate WireGuard implementations on Windows & Android (Wintun vs WireGuardNT vs WireGuard-Go).
- [x] Select permissive, zero-copy, constant-time cryptographic primitives (Monocypher 4.0).
- [x] Produce `ENVIRONMENT_REPORT.md` and `DEPENDENCIES.md`.

---

## Phase 1: Project Foundation & Core Infrastructure
- [ ] Initialize Git repository with proper `.gitignore` and `LICENSE` (Apache 2.0).
- [ ] Set up CMake project with C++20 (`/std:c++20`), warnings-as-errors, and CMake Presets.
- [ ] Vendor / integrate core lightweight dependencies:
  - Monocypher (crypto: X25519, ChaCha20-Poly1305, BLAKE2s)
  - SQLite3 amalgamation + C++ wrapper
  - nlohmann/json
  - GoogleTest or lightweight C++ test harness
- [ ] Implement structured thread-safe logger (`Logger`).
- [ ] Build first minimal executable and unit test suite.
- [ ] Verify clean build with MSVC and Ninja.

---

## Phase 2: Tunnel Foundation & Protocol Engine
- [ ] Implement Curve25519 / X25519 keypair generation and validation.
- [ ] Implement ChaCha20-Poly1305 AEAD authenticated encryption / decryption with 64-bit nonce replay window.
- [ ] Implement BLAKE2s hashing, HMAC, and HKDF.
- [ ] Implement WireGuard message serialization and deserialization:
  - Type 1: Handshake Initiation
  - Type 2: Handshake Response
  - Type 3: Cookie Reply
  - Type 4: Transport Data
- [ ] Implement Noise_IKpsk2 state machine.
- [ ] Two-device loopback integration test verifying encrypted packet exchange and session rotation.

---

## Phase 3: Windows Gateway & Networking Egress
- [ ] Integrate Wintun adapter driver interface (`wintun.h` dynamic loader).
- [ ] Implement virtual TUN adapter initialization (`openroute0`, `10.77.0.1/24`, MTU `1420`).
- [ ] Implement Windows NetNat and IP forwarding orchestrator (`NatForwarder`).
- [ ] Implement LAN isolation packet filter (blocking RFC 1918 traffic from guest).
- [ ] End-to-end data plane loop: TUN read -> Encrypt -> UDP send -> UDP receive -> Decrypt -> TUN write.
- [ ] Integration test: Verify packets route through the gateway and egress to external network.

---

## Phase 4: Windows Client Application
- [ ] Implement Windows client tunnel service.
- [ ] Implement client-side device key management.
- [ ] Implement client connection state machine (`DISCONNECTED`, `CONNECTING`, `CONNECTED`, `RECONNECTING`).
- [ ] Implement DNS protection and fail-closed kill-switch behavior.
- [ ] Connection health, RTT, and traffic statistics.

---

## Phase 5: Android Client Application
- [ ] Setup Android project structure with Kotlin and Gradle wrapper.
- [ ] Integrate `android.net.VpnService` with WireGuard tunnel engine.
- [ ] Implement QR code invitation scanner.
- [ ] Implement VPN permission request flow and foreground notification service.
- [ ] Implement network switching handling (Wi-Fi to mobile data) and auto-reconnect.

---

## Phase 6: Guest Management & Invitation System
- [ ] Cryptographic invitation token generation with expiry and single-use nonces.
- [ ] QR code generator for invitation links.
- [ ] Device enrollment workflow (Pending Approval -> Owner Approval -> Enrolled).
- [ ] Owner approval / reject / pause / revoke commands.
- [ ] Desktop notifications for pending device requests and connection status.

---

## Phase 7: Bandwidth Control & Accounting Engine
- [ ] High-precision per-peer Token Bucket rate limiter (upload and download bps).
- [ ] Real-time atomic byte counters for ingress/egress.
- [ ] Daily and monthly quota enforcement with automatic peer suspension.
- [ ] Temporary guest passes (2 hours, 24 hours, 7 days) with strict expiry timers.
- [ ] Persistent accounting synchronization with SQLite database.

---

## Phase 8: Diagnostics & Remote Connectivity
- [ ] Live performance diagnostics (RTT, packet loss, throughput, gateway CPU/RAM).
- [ ] Gateway upload saturation detection with human-readable guidance.
- [ ] Relay fallback coordination architecture for symmetric NAT topologies.

---

## Phase 9: Desktop User Interface & Hardening
- [ ] Windows Desktop UI implementing the clean blue & white design language.
- [ ] Interactive Dashboard (status, device list, live graph, one-click controls).
- [ ] Security audit, threat model verification, and regression test suite.
- [ ] Packaging and reproducible release build.
