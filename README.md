# OpenRoute

> **Your internet. Shared on your terms.**

OpenRoute is a native Windows application and high-performance gateway engine that transforms an underutilized Wi-Fi or 5G unlimited internet connection into a secure, controlled, and monetized remote internet-sharing service.

Share your high-speed bandwidth with remote users across the city or anywhere in the world, track live data consumption and duration, charge per gigabyte or per hour, and maintain total control with instant permission activations, pausing, and hang-up disconnects.

---

## Highlights

- 🖥️ **Native Windows Desktop App**: Dedicated Win32 desktop application with live real-time metering, device quota monitoring, and one-click controls (no web browser required).
- 💰 **Monetization & Business Model**: Built-in billing engine with customizable rates per GB and per hour (in ₹ INR, $ USD, etc.), real-time earnings ledger, and lifetime revenue analytics.
- 📱 **Phone Dial-In Experience ("Dial to Connect, Hang Up to Stop")**: Remote customers can open the mobile dial-pad on their phone, enter their 6-digit Dial PIN, tap **Call** to activate high-speed internet, and tap **Hang Up** when done.
- 🌐 **Universal Client Access (Zero App Needed)**: Built-in Universal HTTP/HTTPS CONNECT and SOCKS5 proxy engine (port 8080). Works out-of-the-box on **Apple iPhone (iOS), iPad, Android, macOS, Windows laptops, and Linux** using standard system settings without installing any third-party app.
- 🔒 **Standard WireGuard VPN**: Audited WireGuard `Noise_IKpsk2` cryptographic tunnel with an accompanying Android client APK (supporting Android 8.0 through Android 15+).
- 🛡️ **LAN Isolation & Security Guard**: RFC 1918 private IP blocking ensures guests and remote users can access the public internet, but can never snoop on your local home network or private devices.
- 🌍 **Built-in STUN & NAT Traversal**: Automatic discovery of your external Public IP and NAT type for seamless remote connections.

---

## Architecture Overview

```
                          [ Remote Users Anywhere in the World ]
                             /               |               \
                iPhone / iPad (iOS)     Android Phones    Laptops (Win/Mac)
               (Native System Proxy)  (Phone Dial-Pad)   (WireGuard / Proxy)
                             \               |               /
                              ▼              ▼              ▼
                     [ Public Internet / Router Port Forward / Tunnel ]
                                             │
                                             ▼
             +───────────────────────────────────────────────────────────────+
             │           HOST WINDOWS PC (OpenRoute Gateway)                 │
             │                                                               │
             │  ┌─────────────────────────┐   ┌───────────────────────────┐  │
             │  │   Universal Proxy Engine│   │   WireGuard UDP Gateway   │  │
             │  │   (HTTP CONNECT/SOCKS5) │   │   (Noise_IKpsk2 Tunnel)   │  │
             │  │   Port: 8080            │   │   Port: 51820             │  │
             │  └────────────┬────────────┘   └─────────────┬─────────────┘  │
             │               │                              │                │
             │  ┌────────────┴──────────────────────────────┴─────────────┐  │
             │  │  LAN Isolation Guard (RFC 1918 Local Network Protection)│  │
             │  └────────────────────────────┬────────────────────────────┘  │
             │                               │                               │
             │  ┌────────────────────────────┴────────────────────────────┐  │
             │  │  Billing Manager & Quota Engine (Real-Time Accounting)  │  │
             │  └────────────────────────────┬────────────────────────────┘  │
             │                               │                               │
             │  ┌────────────────────────────┴────────────────────────────┐  │
             │  │  Native Windows GUI Desktop App (Live Dashboard & Pins) │  │
             │  └─────────────────────────────────────────────────────────┘  │
             +───────────────────────────────────────────────────────────────+
                                             │
                                             ▼
                             [ Host Unlimited Wi-Fi / 5G Uplink ]
```

---

## Windows Desktop Application

When you start OpenRoute on your host Windows PC, the native desktop window opens automatically:

```
+--------------------------------------------------------------------------------------------------+
| OPENROUTE GATEWAY  [● LIVE - SHARING 5G/WI-FI]  |  Public: 103.x.x.x:8080  |  Revenue: ₹ 145.50  |
+--------------------------------------------------------------------------------------------------+
| [ Live Calls & Devices ]      [ Business & Dial PINs ]      [ Universal Connection Guide ]       |
+--------------------------------------------------------------------------------------------------+
| Device / Phone   Dial PIN   Platform   Client IP       Data Used   Duration   Billed     Status  |
| iPhone-Alice     849-201    iOS        49.37.112.5     1.42 GB     01:14:20   ₹ 71.00    ACTIVE  |
| Android-Bob      912-304    Android    152.58.44.8     620 MB      00:35:10   ₹ 31.00    ACTIVE  |
+--------------------------------------------------------------------------------------------------+
| [ Approve / Activate ]   [ Pause Call ]   [ Hang Up / Disconnect ]             [ Refresh List ]  |
+--------------------------------------------------------------------------------------------------+
```

### App Capabilities
1. **Live Calls & Sessions**: Real-time table updating every second with data consumed (MB/GB), connection duration, and live billed revenue.
2. **Approve / Activate**: One-click approval for new clients awaiting activation.
3. **Pause / Resume**: Temporarily suspend a client's internet routing without removing their pass.
4. **Hang Up / Disconnect**: Immediately terminates the client's session, severs the proxy/tunnel socket, and finalizes the billing transaction in the SQLite database.
5. **Business & Dial PIN Generator**:
   - Configure pricing: Rate per GB (e.g. `₹ 50.00` or `$ 1.00`) and Rate per Hour.
   - Generate numeric Dial PIN passes (e.g. `849-201`) with configurable validity (1 hour to unlimited) and data quota (500 MB to unlimited).
   - **Copy Shareable WhatsApp / SMS Message**: 1-click button that copies complete, pre-formatted client connection instructions directly to your Windows clipboard.

---

## Universal Client Connection Guide (Any Device)

Remote users who are far away from your home network can connect to your gateway using any of the following methods:

### Method 1: Zero Apps Required (Built into iOS, Android, Mac & Windows)
No software installation required on the client device:

- **Apple iPhone / iPad (iOS)**:
  1. Open **Settings** → **Wi-Fi** → Tap the **(i)** icon next to the connected network.
  2. Scroll down to **Configure Proxy** → Select **Manual**.
  3. Enter **Server**: `<Your_Host_Public_IP>` and **Port**: `8080`.
  4. Enable **Authentication** → Set **Password** to the client's **Dial PIN** (e.g., `849201`).
  5. Tap **Save**. All internet traffic now routes through your host connection.

- **Android Phones**:
  1. Open **Settings** → **Network & Internet** → **Wi-Fi** → Tap current Wi-Fi → **Advanced Options**.
  2. Select **Proxy** → **Manual** → Host: `<Your_Host_Public_IP>`, Port: `8080`.
  3. When prompted, enter the Dial PIN.

- **Windows / macOS Laptops**:
  - Windows: **Settings** → **Network & Internet** → **Proxy** → Set manual proxy to `<Your_Host_Public_IP>:8080`.
  - macOS: **System Settings** → **Network** → **Proxies** → Enable **Web Proxy (HTTP)** & **Secure Web Proxy (HTTPS)** with `<Your_Host_Public_IP>:8080`.

---

### Method 2: Phone Dial-Pad Web Portal ("Dial to Connect")
Customers can manage their connection like a phone call:

1. Open a mobile browser on the phone and navigate to:
   ```
   http://<Your_Host_Public_IP>:8080/dial
   ```
2. A smartphone keypad appears on the screen.
3. The customer types their 6-digit **Dial PIN** (e.g., `849201`) and taps the green **CALL** button.
4. The call status updates to:
   ```
   CALL CONNECTED - 5G INTERNET ACTIVE
   Data Streamed: 142.5 MB | Duration: 00:12:45 | Billed: ₹ 7.12
   ```
5. When the user taps the red **HANG UP** button, their internet tunnel stops immediately.

---

### Method 3: WireGuard VPN & Android App
For customers who prefer an encrypted system-wide VPN tunnel:
- **Android App**: Sideload `apps/android/openroute.apk` (compatible with Android 8.0 through Android 15).
- **Standard WireGuard Client**: Import the generated `.conf` or scan the invitation QR code in the official WireGuard app on iOS, Android, Windows, Mac, or Linux.

---

## Remote Access & NAT Traversal (Connecting from Far Away)

When remote users are away from your home router (e.g., on cellular 4G/5G data or another Wi-Fi network):

1. **Auto-Detected Public IP**: OpenRoute automatically discovers your external IP using built-in RFC 5389 STUN (`stun.l.google.com`) and displays it in the header banner.
2. **Standard Home Wi-Fi Routers**:
   - Forward TCP port `8080` (Universal Proxy) and UDP port `51820` (WireGuard) in your router's port forwarding settings to your PC's local IP address (e.g., `192.168.0.239`).
3. **Behind CGNAT / Mobile 5G Hotspots (No Public Port Forwarding)**:
   - Run a free tunnel such as [Cloudflare Tunnel](https://developers.cloudflare.com/cloudflare-one/connections/connect-networks/) or [ngrok](https://ngrok.com/):
     ```bash
     cloudflared tunnel --url http://localhost:8080
     ```
   - Share your assigned tunnel hostname (e.g., `https://your-tunnel.trycloudflare.com`) with your remote users.

---

## Building from Source

### Prerequisites
- Windows 10 / 11 (x64)
- Visual Studio 2022 (with MSVC C++20 toolset)
- CMake 3.25+ and Ninja

### Build Steps

Clone the repository and build:
```powershell
# Quick one-command build using the build script:
.\build_all.bat

# Or manual CMake build:
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=RelWithDebInfo
ninja -C build
```

### Running the Application

```powershell
# Launch the Native Windows Desktop GUI Application:
.\build\bin\openroute_gateway.exe

# Launch in headless console-only mode (for background servers):
.\build\bin\openroute_gateway.exe --headless

# Optional arguments:
#   --port <port>       Change HTTP server port (default: 8765)
#   --db <path>         Specify custom SQLite database path
#   --open-browser      Also open the web dashboard in your default browser
```

---

## Automated Verification & Tests

OpenRoute includes a full automated test suite covering cryptography, SQLite migrations, database transactions, billing and ledger calculations, WireGuard Noise handshakes, anti-replay protections, token bucket rate limiting, STUN NAT classification, relay framing, and HTTP endpoints:

```powershell
.\build\bin\openroute_unit_tests.exe
```

Output:
```
=======================================================
[==========] Running 33 test(s).
[  PASSED  ] 33 test(s) (2508 ms total).
=======================================================
```

---

## Project Structure

```
OpenRoute/
├── apps/
│   ├── android/              # Native Android VPN Client (APK, Java, API 26-34)
│   └── windows/              # Native Windows Desktop GUI App (Win32, C++20)
├── core/
│   ├── accounting/           # Billing Manager, Pricing Rates, Call Sessions, Quotas
│   ├── bandwidth/            # Token bucket bandwidth shaping
│   ├── common/               # Logging, endian utilities, shared types
│   ├── device/               # Device manager and approval gate
│   ├── diagnostics/          # Health metrics, packet loss, RTT probers
│   ├── gateway/              # Universal Proxy (HTTP CONNECT/SOCKS5), Wintun routing
│   ├── networking/           # RFC 5389/8489 STUN NAT traversal engine
│   ├── policy/               # Invitation passes and access policies
│   ├── relay/                # UDP packet relay framing
│   ├── security/             # Monocypher (ChaCha20-Poly1305, X25519, BLAKE2s, DPAPI)
│   ├── storage/              # SQLite database wrapper and automated migrations
│   ├── tunnel/               # Noise_IKpsk2 state machine and replay windows
│   └── ui/                   # Embedded HTTP server and mobile Phone Dial-Pad
├── docs/                     # Architectural, security, and networking documentation
├── tests/                    # Unit and integration test suite
├── build_all.bat             # Fast MSVC Ninja build script
├── build_apk.ps1             # Android APK build and v2/v3 signing script
└── CMakeLists.txt            # Root build definition
```

---

## License

Licensed under the [Apache License, Version 2.0](LICENSE).
