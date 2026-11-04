# OpenRoute — Threat Model & Security Analysis

## 1. Threat Modeling Methodology

OpenRoute follows the **STRIDE** (Spoofing, Tampering, Repudiation, Information Disclosure, Denial of Service, Elevation of Privilege) methodology combined with a defense-in-depth model tailored to peer-to-peer and internet-sharing VPN architectures.

---

## 2. Threat Actors & Assumptions

| Threat Actor | Capabilities & Motivation | Assumed Trust Level |
| :--- | :--- | :--- |
| **Untrusted Network Observer** | Controls local Wi-Fi, ISP, or intermediate routers. Can sniff, drop, reorder, or inject packets. | Zero trust. Must never read tunnel plaintext or tamper with traffic undetected. |
| **Malicious Guest Device** | Has legitimate access to the OpenRoute tunnel. May attempt to port-scan the gateway host's private LAN, mount DDoS attacks, or consume excessive bandwidth. | Untrusted boundary. Must be isolated from host LAN and strictly bounded by rate limits and quotas. |
| **Unauthorized Third Party** | Intercepts an invitation link, eavesdrops on QR transmission, or tries brute-force connection attempts. | Hostile. Prevented by approval gates, single-use tokens, and cryptographic handshakes. |
| **Compromised Gateway Host** | Malware present on the host PC or physical theft. | Host security boundary. Mitigation: Gateway private keys protected by Windows DPAPI; administrative interface restricted to localhost. |

---

## 3. Threat Analysis & Mitigations

### 3.1 Malicious Invited Devices & LAN Exposure
- **Threat**: An invited guest device connects to the gateway and attempts to scan, exploit, or pivot into devices on the gateway owner's home network (e.g., smart home devices, NAS, local routers at `192.168.0.x`).
- **Mitigation**:
  1. **Strict LAN Isolation Filter**: The gateway data plane inspects every packet exiting the TUN interface. All destination IPs falling into RFC 1918 ranges (`192.168.0.0/16`, `10.0.0.0/8`, `172.16.0.0/12`) that match the gateway's local subnet are dropped by default.
  2. Traffic is strictly forwarded to the default gateway's public WAN interface through Windows NetNat.
  3. No inter-peer routing (Peer A cannot communicate with Peer B across the tunnel unless explicitly authorized).

### 3.2 Stolen or Leaked Invitation Links
- **Threat**: An invitation QR code or URL sent via chat is forwarded to an unauthorized party.
- **Mitigation**:
  1. **Owner Approval Gate**: Generating an invitation does not immediately activate tunnel credentials. When a device redeems an invitation, it enters `PENDING_APPROVAL` status. The gateway owner receives a notification and must explicitly click "Approve" on the desktop UI before the public key is registered in the tunnel.
  2. **One-Time Token Redemptions**: Invitation tokens are invalidated immediately upon first enrollment.
  3. **Strict Expiration**: Invitations automatically expire after a configurable duration (default: 15 minutes for onboarding, or specific temporary pass duration).

### 3.3 Denial of Service & Resource Exhaustion
- **Threat**: An external attacker floods the gateway's UDP port (51820) with bogus initiation packets to exhaust memory and CPU (cryptographic handshake flood).
- **Mitigation**:
  1. **WireGuard Cookie Mechanism (Noise_IKpsk2)**: Under normal load, initiation packets are processed with lightweight X25519 scalar multiplications. Under heavy load, the gateway responds with a MAC2 Cookie Reply containing an authenticated token generated using a rotating secret. The client must re-transmit the handshake with proof of work / cookie, preventing state exhaustion.
  2. **Rate Limiting Handshakes**: Handshake attempts per remote IP are rate-limited.
  3. **Per-Device Token Bucket**: Connected devices are throttled to configured bandwidth ceilings, preventing any single guest from saturating the gateway owner's ISP upload pipe.

### 3.4 Traffic Interception & Cryptographic Weaknesses
- **Threat**: Eavesdropping or replay of tunnel packets.
- **Mitigation**:
  1. **Standard Modern Cryptography**: Standard WireGuard protocol using X25519 ECDH, ChaCha20 stream cipher, Poly1305 authenticator, and BLAKE2s hashing.
  2. **Replay Protection**: 64-bit monotonically increasing packet counter with a 128-packet sliding replay window.
  3. **Frequent Key Rotation**: Ephemeral session keys rotate every 120 seconds or after $2^{64}-1$ packets.

### 3.5 DNS Leaks & Fail-Closed Behavior
- **Threat**: When connected to the gateway, client DNS requests bypass the tunnel and leak to the local ISP, exposing browsing habits.
- **Mitigation**:
  1. **Client DNS Enforcement**: Client tunnel configuration sets DNS server to gateway tunnel IP (`10.77.0.1`) or secure public resolvers (e.g. `1.1.1.1`, `9.9.9.9`) routed exclusively via the tunnel interface.
  2. **Fail-Closed (Kill-Switch)**: If the tunnel connection drops or is terminated, client routing policies block outbound non-tunnel traffic until reconnect or explicit user disconnection.

### 3.6 Administrative Interface Exposure
- **Threat**: Remote guest devices attempt to access the OpenRoute gateway management API or desktop interface.
- **Mitigation**:
  1. Control plane IPC and management interfaces bind strictly to `127.0.0.1` (localhost) or Windows Named Pipes with strict DACLs (Discretionary Access Control Lists) allowing access only to the local administrator / current logged-in user.
  2. Tunnel IP interface (`10.77.0.1`) rejects all incoming connections to the administrative port.
