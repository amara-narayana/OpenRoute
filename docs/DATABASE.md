# OpenRoute — Database Architecture & Data Model

## 1. Overview & Technology Selection

- **Database Engine**: SQLite 3 (Amalgamation, embedded in-process).
- **Storage Path**: `%APPDATA%\OpenRoute\openroute.db` (Windows) or `/data/data/org.openroute/databases/openroute.db` (Android).
- **Concurrency Mode**: Write-Ahead Logging (`PRAGMA journal_mode=WAL;`), synchronous normal (`PRAGMA synchronous=NORMAL;`), foreign keys enabled (`PRAGMA foreign_keys=ON;`).

---

## 2. Entity-Relationship Schema

```mermaid
erDiagram
    GATEWAYS ||--o{ DEVICES : owns
    GATEWAYS ||--o{ INVITATIONS : issues
    DEVICES ||--o{ SESSIONS : establishes
    DEVICES ||--o{ USAGE_RECORDS : generates
    DEVICES ||--o| POLICIES : assigns
    GATEWAYS ||--o{ AUDIT_LOGS : records

    GATEWAYS {
        text id PK
        text name
        text public_key
        text encrypted_private_key
        integer listen_port
        text tunnel_subnet
        text egress_interface
        integer is_active
        integer created_at
        integer updated_at
    }

    DEVICES {
        text id PK
        text gateway_id FK
        text name
        text platform
        text public_key
        text assigned_ip
        text status
        integer approved_at
        integer expires_at
        integer created_at
    }

    POLICIES {
        text id PK
        text device_id FK
        integer max_down_bps
        integer max_up_bps
        integer daily_quota_bytes
        integer monthly_quota_bytes
        integer allow_lan_access
        text schedule_json
    }

    INVITATIONS {
        text id PK
        text gateway_id FK
        text token_hash
        text target_platform
        text pass_type
        integer max_uses
        integer current_uses
        integer expires_at
        integer is_revoked
        integer created_at
    }

    USAGE_RECORDS {
        text id PK
        text device_id FK
        integer timestamp_epoch
        integer bytes_rx
        integer bytes_tx
        integer duration_seconds
    }

    SESSIONS {
        text id PK
        text device_id FK
        text remote_endpoint
        integer connected_at
        integer disconnected_at
        integer bytes_rx
        integer bytes_tx
        text disconnect_reason
    }

    AUDIT_LOGS {
        text id PK
        text gateway_id FK
        text event_type
        text severity
        text description
        text metadata_json
        integer created_at
    }
```

---

## 3. SQL DDL Specification

```sql
-- Schema Version 1

CREATE TABLE IF NOT EXISTS schema_migrations (
    version INTEGER PRIMARY KEY,
    applied_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS gateways (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    public_key TEXT NOT NULL UNIQUE,
    encrypted_private_key BLOB NOT NULL,
    listen_port INTEGER NOT NULL DEFAULT 51820,
    tunnel_subnet TEXT NOT NULL DEFAULT '10.77.0.0/24',
    egress_interface TEXT NOT NULL DEFAULT 'Wi-Fi',
    is_active INTEGER NOT NULL DEFAULT 1,
    created_at INTEGER NOT NULL,
    updated_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS devices (
    id TEXT PRIMARY KEY,
    gateway_id TEXT NOT NULL REFERENCES gateways(id) ON DELETE CASCADE,
    name TEXT NOT NULL,
    platform TEXT NOT NULL, -- 'android', 'windows', etc.
    public_key TEXT NOT NULL UNIQUE,
    assigned_ip TEXT NOT NULL UNIQUE,
    status TEXT NOT NULL DEFAULT 'PENDING_APPROVAL', -- 'PENDING_APPROVAL', 'APPROVED', 'PAUSED', 'REVOKED', 'EXPIRED'
    approved_at INTEGER,
    expires_at INTEGER,
    created_at INTEGER NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_devices_gateway ON devices(gateway_id);
CREATE INDEX IF NOT EXISTS idx_devices_public_key ON devices(public_key);
CREATE INDEX IF NOT EXISTS idx_devices_status ON devices(status);

CREATE TABLE IF NOT EXISTS policies (
    id TEXT PRIMARY KEY,
    device_id TEXT NOT NULL UNIQUE REFERENCES devices(id) ON DELETE CASCADE,
    max_down_bps INTEGER NOT NULL DEFAULT 0, -- 0 = unlimited
    max_up_bps INTEGER NOT NULL DEFAULT 0,   -- 0 = unlimited
    daily_quota_bytes INTEGER NOT NULL DEFAULT 0, -- 0 = unlimited
    monthly_quota_bytes INTEGER NOT NULL DEFAULT 0,
    allow_lan_access INTEGER NOT NULL DEFAULT 0,
    schedule_json TEXT
);

CREATE TABLE IF NOT EXISTS invitations (
    id TEXT PRIMARY KEY,
    gateway_id TEXT NOT NULL REFERENCES gateways(id) ON DELETE CASCADE,
    token_hash TEXT NOT NULL UNIQUE,
    target_platform TEXT NOT NULL,
    pass_type TEXT NOT NULL DEFAULT 'TEMPORARY_24H',
    max_uses INTEGER NOT NULL DEFAULT 1,
    current_uses INTEGER NOT NULL DEFAULT 0,
    expires_at INTEGER NOT NULL,
    is_revoked INTEGER NOT NULL DEFAULT 0,
    created_at INTEGER NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_invitations_token_hash ON invitations(token_hash);

CREATE TABLE IF NOT EXISTS usage_records (
    id TEXT PRIMARY KEY,
    device_id TEXT NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
    timestamp_epoch INTEGER NOT NULL,
    bytes_rx INTEGER NOT NULL DEFAULT 0,
    bytes_tx INTEGER NOT NULL DEFAULT 0,
    duration_seconds INTEGER NOT NULL DEFAULT 0
);

CREATE INDEX IF NOT EXISTS idx_usage_device_time ON usage_records(device_id, timestamp_epoch);

CREATE TABLE IF NOT EXISTS audit_logs (
    id TEXT PRIMARY KEY,
    gateway_id TEXT NOT NULL REFERENCES gateways(id) ON DELETE CASCADE,
    event_type TEXT NOT NULL,
    severity TEXT NOT NULL DEFAULT 'INFO',
    description TEXT NOT NULL,
    metadata_json TEXT,
    created_at INTEGER NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_audit_time ON audit_logs(created_at);
```
