# OpenRoute — Security Guidelines & Best Practices

## 1. Security Architecture Principles

1. **Defense in Depth**: Security controls operate at multiple layers: network packet inspection, cryptographic handshake verification, local authorization policies, and OS filesystem protection.
2. **Zero Cryptographic Invention**: All cryptosystems follow standard, audited implementations of WireGuard Noise_IKpsk2, RFC 7748 (Curve25519), RFC 8439 (ChaCha20-Poly1305), and RFC 7693 (BLAKE2s).
3. **Privilege Minimization**: Core data plane functions that interact with the network stack require Windows Administrator privileges for Wintun adapter creation and NetNat configuration, but run without opening unauthenticated network listening ports beyond the single WireGuard UDP port.
4. **No Plaintext Credential Persistence**: Private keys, preshared keys, and sensitive tokens are encrypted using platform cryptographic APIs.

---

## 2. Key Management & Storage

### 2.1 Gateway & Device Keypairs
- **Key Generation**: Cryptographically secure pseudorandom numbers generated via OS system entropy (`BCryptGenRandom` on Windows, `/dev/urandom` / `SecRandomCopy` on Unix/Android).
- **At-Rest Protection on Windows**:
  - Keys stored in SQLite or config files are encrypted using Windows Data Protection API (`CryptProtectData` - DPAPI).
  - DPAPI binds encryption to the local Windows user credentials and machine TPM/LSA secrets, ensuring keys cannot be extracted by copying the database file to another machine.
- **In-Memory Hygiene**:
  - Sensitive buffers containing private keys or decrypted key materials are scrubbed from memory using `SecureZeroMemory` or `explicit_bzero` immediately when out of scope.

---

## 3. LAN Isolation & Firewall Rules

To protect the gateway owner from unauthorized access by guests:
1. **Windows Firewall Rule Creation**:
   - Inbound Rule: Allow UDP traffic on OpenRoute listen port (default 51820) from Any.
   - Forwarding Rule: Traffic arriving on `openroute0` interface is permitted outbound only via NAT to the external WAN adapter.
2. **Packet Filter Enforcement**:
   - Any IP packet arriving on the tunnel interface destined for IPv4 RFC 1918 addresses (`10.0.0.0/8`, `172.16.0.0/12`, `192.168.0.0/16`) or link-local (`169.254.0.0/16`) matching the host's physical network is discarded with an ICMP Host Unreachable response or dropped silently.

---

## 4. Reporting Vulnerabilities

If you discover a security vulnerability in OpenRoute:
- Do not open a public issue.
- Submit a detailed report outlining reproduction steps, potential impact, and proof-of-concept to security@openroute.local.
- Disclosures will be acknowledged within 48 hours and patched in high-priority releases.
