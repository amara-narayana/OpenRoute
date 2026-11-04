# OpenRoute

> **Your internet. Shared on your terms.**

OpenRoute is a controlled remote internet-sharing platform that turns a Windows PC into a secure, remotely accessible internet gateway. Share your connection with approved Android and Windows devices while maintaining granular ownership, bandwidth shaping, data quotas, and instantaneous access revocation.

---

## Key Differentiators

- **Guest-First Sharing**: One-click onboarding via QR codes and invitation links.
- **Owner Approval Gate**: Devices cannot route traffic without explicit owner authorization.
- **LAN Isolation**: Guests can access the internet, but cannot snoop on your local home network or private LAN devices.
- **Per-Device Bandwidth Shaping**: Token bucket rate limiting ensures guests cannot saturate your uplink.
- **Granular Data Quotas**: Daily and monthly usage caps with automatic cutoff.
- **Temporary Passes**: Configurable 2-hour, 24-hour, 7-day, or custom passes that expire automatically.
- **WireGuard Standard Protocol**: Built upon standard, audited WireGuard Noise_IKpsk2 cryptography.
- **Real Diagnostics**: Honest, unvarnished network health metrics (RTT, packet loss, bandwidth).

---

## Architectural Documentation

Detailed engineering specifications are available in the [`docs/`](docs/) directory:
- [Architecture Overview](docs/ARCHITECTURE.md)
- [Threat Model & Security Analysis](docs/THREAT_MODEL.md)
- [Security Guidelines](docs/SECURITY.md)
- [Networking & Packet Pipeline](docs/NETWORKING.md)
- [Database Schema & Data Model](docs/DATABASE.md)
- [Implementation Roadmap](docs/ROADMAP.md)
- [Build Instructions](docs/BUILD.md)
- [Testing Verification Plan](docs/TESTING.md)
- [Environment Inspection Report](docs/ENVIRONMENT_REPORT.md)
- [Dependencies & Licenses](docs/DEPENDENCIES.md)
- [Implementation Status](docs/IMPLEMENTATION_STATUS.md)

---

## Quick Start (Building from Source)

Ensure you have CMake 3.25+, Visual Studio 2022 (C++20), and Ninja installed:

```powershell
# Configure build
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=RelWithDebInfo

# Compile
cmake --build build -j 2

# Run automated tests (31/31 passing)
ctest --test-dir build --output-on-failure

# Launch OpenRoute Gateway & Web Control Dashboard
.\build\bin\openroute_gateway.exe
```

When started, OpenRoute Gateway automatically hosts the local control dashboard at `http://127.0.0.1:8765` and opens it in your default browser. To run headlessly:
```powershell
.\build\bin\openroute_gateway.exe --no-browser --port 8765
```

---

## License

Licensed under the [Apache License, Version 2.0](LICENSE).
