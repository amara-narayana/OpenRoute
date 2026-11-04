# OpenRoute — Testing Strategy & Verification Plan

## 1. Testing Pyramid

OpenRoute enforces rigorous automated testing across three tiers:

1. **Unit Tests (`tests/unit`)**:
   - Cryptographic primitives (X25519 key exchange, ChaCha20-Poly1305 encryption/decryption, BLAKE2s hashing).
   - WireGuard packet serializing/deserializing (Types 1, 2, 3, 4).
   - Token-bucket rate limiter math and edge conditions.
   - Quota counter arithmetic and reset schedules.
   - Invitation token generation, hashing, and expiration checks.
   - SQLite CRUD operations, constraints, and migrations.

2. **Integration Tests (`tests/integration`)**:
   - Full Noise_IKpsk2 handshake loopback between virtual client and virtual gateway over loopback UDP sockets.
   - Data plane transport packet encapsulation, sequence number replay window, and decryption.
   - NAT forwarder configuration generation and verification.
   - LAN isolation filter verification (confirming RFC 1918 packets are intercepted).

3. **Performance & Security Tests (`tests/performance`, `tests/security`)**:
   - Token bucket throughput under sustained burst traffic.
   - Replay attack rejection test.
   - Cryptographic invalid MAC injection test.
   - Memory leak and resource cleanup checks under repeated connect/disconnect cycles.

---

## 2. Running Test Suites

Execute test suites via CTest or directly:
```powershell
ctest --test-dir build --output-on-failure
```
Or run the individual test binaries:
```powershell
./build/bin/openroute_unit_tests.exe
./build/bin/openroute_integration_tests.exe
```
