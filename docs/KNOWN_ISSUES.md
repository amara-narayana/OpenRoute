# OpenRoute — Known Issues & Technical Constraints

## 1. System & Hardware Constraints
- **Host Physical Memory**: The host machine has ~8 GB of RAM with variable available headroom (~700MB - 1GB free).
  - *Impact*: Compiling large monolithic templates in parallel can exhaust memory.
  - *Workaround*: Parallel compilation is bounded to `-j2` or `-j4` in build presets.

## 2. Windows Networking Quirks
- **NetNat Single Prefix Limitation**: Windows supports one internal NAT prefix at a time on client editions of Windows 10/11.
  - *Mitigation*: The gateway checks for existing NetNat configurations before attempting creation, and provides clean teardown on exit.
- **Administrator Privileges for Wintun**: Creating network adapters and modifying IP routing requires elevated (Run as Administrator) privileges.
  - *Mitigation*: Application checks elevation at startup and provides friendly guidance when running unelevated.
