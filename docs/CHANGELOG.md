# OpenRoute — Changelog

All notable changes to this project will be documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [0.1.0-alpha] - 2026-11-04

### Added
- **Core Cryptography**:
  - Constant-time BLAKE2s-256 (RFC 7693), Keyed MAC, and HKDF-1/2/3 in `core/security/blake2s.hpp|.cpp`.
  - Curve25519 / X25519 ECDH keypair generation, Diffie-Hellman secret exchange, and ChaCha20-Poly1305 AEAD authenticated encryption using Monocypher in `core/security/crypto.hpp|.cpp`.
  - Windows DPAPI at-rest key protection for persistent private keys.
  - Zero-allocation memory wipe in `KeyPair` destructor.
- **SQLite Storage Subsystem**:
  - Thread-safe SQLite3 wrapper (`core/storage/database.hpp|.cpp`) with WAL mode, prepared statements, and transactional safety.
  - Migration engine applying Version 1 schema: `gateways`, `devices`, `device_policies`, `invitations`, `usage_accounting`, `audit_logs`.
- **WireGuard Protocol Engine (Data Plane)**:
  - WireGuard protocol definitions: Type 1 Initiation (148 bytes), Type 2 Response (92 bytes), Type 3 Cookie Reply (64 bytes), Type 4 Transport Data encapsulation in `core/tunnel/wireguard_types.hpp`.
  - Full Noise_IKpsk2 state machine in `core/tunnel/noise_protocol.hpp|.cpp`.
  - 128-bit sliding anti-replay bitmap window in `core/tunnel/replay_window.hpp|.cpp`.
  - Peer session key rotation and keepalive tracker in `core/tunnel/peer_session.hpp|.cpp`.
  - Dynamic loader for official `wintun.dll` v0.14.1 in `core/tunnel/wintun_adapter.hpp|.cpp`.
- **Gateway Controls & Policies**:
  - RFC 1918 Private LAN Isolation filter in `core/gateway/lan_isolation.hpp|.cpp`, dropping unauthorized access to gateway's local smart devices and router.
  - Windows NetNat orchestrator and network adapter enumerator in `core/gateway/nat_forwarder.hpp|.cpp`.
  - Microsecond token bucket rate limiter in `core/bandwidth/token_bucket.hpp|.cpp` for upload and download traffic shaping.
  - Quota and pass accounting engine in `core/accounting/quota_manager.hpp|.cpp` with 80% warning threshold, 100% hard packet cutoff, and SQLite persistence.
  - Device lifecycle manager in `core/device/device_manager.hpp|.cpp` supporting 5 states: `PENDING_APPROVAL`, `APPROVED`, `PAUSED`, `REVOKED`, `EXPIRED`.
  - Cryptographic invitation manager in `core/policy/invitation_manager.hpp|.cpp` generating 256-bit entropy tokens, pass durations (2h, 24h, 7d, unlimited), and `openroute://` URIs with single-use protection.
  - Complete gateway orchestrator in `core/gateway/gateway_server.hpp|.cpp`.
- **Client Engines**:
  - Windows Client Engine (`core/tunnel/client_engine.hpp|.cpp`) with URL parser, handshake initiator, and fail-closed kill switch.
  - Complete Android mobile client in Kotlin (`apps/android`): `OpenRouteVpnService.kt`, `WireGuardCrypto.kt`, `QrScannerActivity.kt`, Material3 Blue/White dashboard `MainActivity.kt`.
- **Diagnostics & Remote Connectivity (Phase 8)**:
  - RFC 5389 / RFC 8489 STUN prober in `core/networking/nat_probe.hpp|.cpp` with XOR-MAPPED-ADDRESS parsing and NAT classification (Direct Public, Full Cone, Symmetric NAT).
  - Authenticated relay client coordinator in `core/relay/relay_client.hpp|.cpp` with `0x4F50524C` framing for Symmetric NAT fallback.
  - Telemetry sampler in `core/diagnostics/diagnostics_engine.hpp|.cpp` measuring smoothed RTT, jitter, loss %, and Windows 11 host CPU/RAM metrics.
- **Desktop Dashboard & UI (Phase 9)**:
  - Single-Page Application (SPA) dashboard in `apps/windows/dashboard_assets.hpp` adhering to the Blue & White design palette (`#2563EB`, `#FFFFFF`, `#F8FAFC`, `#0F172A`).
  - Embedded high-performance HTTP & REST API server in `core/ui/http_server.hpp|.cpp` listening on `127.0.0.1:8765`.
  - Real-time network throughput canvas graph, interactive bandwidth sliders, offline SVG QR code generator, and one-click device approval queue.
- **Smart QR Onboarding & Auto-App Download (Phase 10)**:
  - Seamless camera QR flow: standard camera or Google Lens scan points to `http://<lan-ip>:8765/join?token=...`.
  - Auto-download landing page in `apps/windows/join_assets.hpp` automatically initiates APK download after 600ms, provides fallback download buttons, and enables direct deep linking (`openroute://invite?...`) for pre-installed clients.
  - Multi-packet binary streaming server with chunked TCP writes, `SD_SEND` connection shutdown, and `HEAD` request support.
  - Pre-compiled standalone signed Android client APK (`apps/android/openroute.apk`, 34KB) packaged with v1/v2/v3 signatures, zero external dependencies, and native Android `VpnService` support.
- **Automated Test Suite**:
  - 31 automated test cases passing with 100% success rate across crypto, database, tunnel handshake, gateway pipeline, client engine, NAT probing, relay framing, and HTTP REST server.
