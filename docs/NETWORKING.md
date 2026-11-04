# OpenRoute — Networking Architecture & Specification

## 1. Subnet & Addressing Plan

OpenRoute allocates a dedicated virtual private IPv4 subnet for the gateway tunnel:

- **Default Subnet**: `10.77.0.0/24` (254 host addresses)
- **Gateway Tunnel IP**: `10.77.0.1`
- **Subnet Mask**: `255.255.255.0` (`/24`)
- **Peer IP Allocation Range**: `10.77.0.2` through `10.77.0.254`
- **Broadcast Address**: `10.77.0.255`
- **Conflict Resolution**:
  - Before initializing the tunnel interface, OpenRoute queries `Get-NetIPAddress` and `Get-NetRoute`.
  - If `10.77.0.0/24` collides with any physical or virtual adapter on the host, OpenRoute automatically probes alternate candidate subnets:
    - Secondary: `10.78.0.0/24`
    - Tertiary: `172.29.77.0/24`
    - Quaternary: `192.168.77.0/24`

---

## 2. MTU (Maximum Transmission Unit) Sizing

- Physical Ethernet/Wi-Fi standard MTU: `1500` bytes.
- WireGuard packet overhead:
  - IPv4 Header: 20 bytes (or IPv6 Header: 40 bytes)
  - UDP Header: 8 bytes
  - WireGuard Transport Header: 16 bytes (Type: 4 bytes, Receiver index: 4 bytes, Counter: 8 bytes)
  - Poly1305 Authentication Tag: 16 bytes
  - Total overhead over IPv4: 60 bytes (over IPv6: 80 bytes).
- **OpenRoute Tunnel MTU**: `1420` bytes.
  - Sized conservatively to guarantee zero fragmentation even across IPv6 encapsulation or PPPoE uplinks (which use 1492 MTU).
  - TCP MSS Clamping: Clamped to `1380` bytes (`MTU - 40`) to prevent path MTU discovery black holes.

---

## 3. Windows 11 NAT & IP Forwarding Mechanics

### 3.1 Network Address Translation (NetNat)
Windows 10/11 includes built-in kernel NAT managed via the `NetNat` PowerShell cmdlets and WFP:
```powershell
# Create or verify NetNat instance for the OpenRoute tunnel prefix
New-NetNat -Name "OpenRouteNat" -InternalIPInterfaceAddressPrefix "10.77.0.0/24"
```

### 3.2 IP Forwarding
IP forwarding must be enabled on both the virtual tunnel adapter and the internet-facing physical adapter (e.g. Wi-Fi or Ethernet):
```powershell
# Enable forwarding on tunnel interface
Set-NetIPInterface -InterfaceAlias "openroute0" -Forwarding Enabled

# Enable forwarding on physical internet interface (e.g. Wi-Fi)
Set-NetIPInterface -InterfaceAlias "Wi-Fi" -Forwarding Enabled
```

---

## 4. Packet Flow & Processing Pipeline

```
[Remote Client] (e.g. 10.77.0.2)
       │
       │ Encrypted UDP Packet (Port 51820)
       ▼
[OpenRoute UDP Socket]
       │
       │ Decrypt & Authenticate (ChaCha20-Poly1305 + BLAKE2s)
       ▼
[Bandwidth Token Bucket & Quota Check]
       │
       │ If Quota Exceeded -> DROP
       │ If Rate Exceeded  -> Throttle / Buffer
       ▼
[LAN Isolation Filter]
       │
       │ Destination == RFC 1918 Private LAN? -> DROP
       ▼
[Write to Wintun Ring Buffer (10.77.0.1)]
       │
       ▼
[Windows Kernel IP Routing & NetNat Engine]
       │
       │ Source NAT: 10.77.0.2 -> Host IP (e.g. 192.168.0.239)
       ▼
[Physical Wi-Fi / WAN Adapter]
       │
       ▼
[Public Internet Server]
```

### Return Path:
1. Public Internet responds to Host IP (`192.168.0.239`).
2. Windows NetNat matches state table and translates destination IP to `10.77.0.2`.
3. Packet is routed to virtual adapter `openroute0`.
4. OpenRoute Gateway reads raw packet from Wintun ring buffer.
5. Ingress bandwidth limiter tokens are checked.
6. Packet is encrypted with peer's current session key.
7. Encrypted UDP datagram is transmitted back to client's remote endpoint.
