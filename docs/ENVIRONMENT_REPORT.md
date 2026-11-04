# OpenRoute — Environment & System Inspection Report

**Date of Inspection**: 2026-11-04  
**Target Architecture**: Windows 11 (64-bit), x86_64 Gateway & Client, Android Mobile Client  
**Execution Mode**: Autonomous Native Development  

---

## 1. Host Operating System & Hardware Specs

| Metric | Measured Value | Operational Assessment |
| :--- | :--- | :--- |
| **Operating System** | Microsoft Windows 11 Home Single Language (Build 10.0.26300) | Fully supports Windows NetNat, Wintun driver, Win32 IP Helper, Windows Filtering Platform (WFP), DPAPI. |
| **CPU Architecture** | Intel Core i5 (x86_64, 64-bit) | Supports AVX2, AES-NI, SSE4.1 instructions for high-performance cryptographic operations (ChaCha20, Poly1305, BLAKE2s). |
| **Physical Memory (RAM)** | Total: 8,117,028 KB (~8 GB) \| Free: ~678 MB - 1 GB | **Constrained memory**. Critical guideline: Avoid heavy background virtual machines or bloated tooling. Use Ninja + MSVC with controlled concurrency (`-j2` or `-j4`) to prevent Out-Of-Memory (OOM) compiler paging. |
| **Disk Storage** | Drive C: 33.5 GB free \| Drive D: 876.8 GB free | Workspace is on `D:\Open-router`. Abundant disk space available on D: drive for incremental builds, tests, and caches. |

---

## 2. Toolchain & Runtime Availability

| Component / Tool | Detected Status | Version & Details | Action Taken / Recommendation |
| :--- | :--- | :--- | :--- |
| **Git** | **Available** | `git version 2.55.0.windows.3` | Project repository initialized (`git init -b main`). |
| **CMake** | **Available** | `cmake version 4.4.0` | Native CMake 4.4 used as core build orchestration engine. |
| **Ninja** | **Available** | `1.13.2` (`C:\msys64\ucrt64\bin\ninja.exe`) | Fast, lightweight build generator paired with MSVC / GCC. |
| **MSVC (Visual Studio 2022)** | **Available** | Visual Studio 2022 Community (v17.14), MSVC 19.44 (`cl.exe`), Windows 10/11 SDK | Primary compiler for Windows Gateway and Desktop client. Fully supports C++20 standard (`/std:c++20`), RAII, concepts, and Win32 networking APIs. |
| **MinGW / GCC** | **Available** | `gcc 16.2.0`, `g++ 16.2.0` (MSYS2 UCRT64) | Secondary compiler for cross-verification. |
| **Python** | **Available** | `Python 3.13.0` (`C:\Users\Dell\AppData\Local\Programs\Python\Python313`) | Available for test orchestration, packet generation, and scripts. |
| **Android SDK** | **Available** | Platforms: `android-35`, `android-36` (`C:\Users\Dell\AppData\Local\Android\Sdk`) | Ready for Android client development. |
| **Android NDK** | **Available** | Versions: `26.3.11579264`, `29.0.13599879` | Native C++ tunnel integration available for Android. |
| **Java JDK** | **Not in PATH** | Not directly in standard PATH; Available via Winget (`Microsoft.OpenJDK.17` or `21`) | Will be installed when initiating Phase 5 (Android client build). |
| **Package Manager** | **Available** | `winget` (Windows Package Manager) | Available for non-intrusive dependencies if needed. |

---

## 3. Network Environment & Interfaces

| Adapter Name | Type & Description | Status | Current IPv4 Address | Subnet / Role |
| :--- | :--- | :--- | :--- | :--- |
| **Wi-Fi** | Realtek 8821CE Wireless LAN 802.11ac | **Up** (Connected) | `192.168.0.239` | `192.168.0.0/24` (Egress WAN interface for Gateway) |
| **Ethernet 2** | VirtualBox Host-Only Ethernet Adapter | Up | Virtual Subnet | Isolated host-only testing |
| **Ethernet** | Sophos TAP Adapter | Disconnected | None | Virtual TAP adapter |
| **Bluetooth** | Bluetooth Personal Area Network | Disconnected | None | Inactive |

### Tunnel Subnet Allocation:
- Target tunnel subnet: `10.77.0.0/24`
- Gateway assigned tunnel IP: `10.77.0.1`
- Client assigned tunnel IP range: `10.77.0.2` - `10.77.0.254`
- Conflict check: `10.77.0.0/24` has no overlap with host LAN (`192.168.0.0/24`). Verified collision-free.

---

## 4. Resource Constraints & Mitigation Plan

1. **Memory Budget**: With ~8 GB RAM total and variable free RAM, large monolith builds or running multiple virtual machines will cause severe page thrashing.
   - *Mitigation*: Header-only or small modular translation units. Pin Ninja jobs to `ninja -j 2`. Do not download heavy monolithic frameworks.
2. **Virtualization Restrictions**: No Docker or nested virtualization required.
   - *Mitigation*: Native Windows services and native processes. Windows NetNat and Wintun driver operate directly on the Windows 11 host.
3. **Reproducibility**: Use CMake Presets (`CMakePresets.json`) and vendored, permissive, audited zero-dependency cryptographic primitives (Monocypher).
