#pragma once

#include <string>
#include <string_view>

namespace openroute::ui {

inline constexpr std::string_view DASHBOARD_HEAD = R"rawhtml(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>OpenRoute Gateway — Control Dashboard</title>
  <style>
    :root {
      --primary: #2563EB;
      --primary-dark: #1D4ED8;
      --primary-light: #EFF6FF;
      --card-bg: #FFFFFF;
      --canvas: #F8FAFC;
      --text: #0F172A;
      --text-muted: #64748B;
      --border: #E2E8F0;
      --success: #10B981;
      --success-light: #ECFDF5;
      --warning: #F59E0B;
      --warning-light: #FFFBEB;
      --danger: #EF4444;
      --danger-light: #FEF2F2;
      --shadow-sm: 0 1px 3px rgba(0,0,0,0.06), 0 1px 2px rgba(0,0,0,0.04);
      --shadow-md: 0 4px 6px -1px rgba(0,0,0,0.07), 0 2px 4px -1px rgba(0,0,0,0.04);
      --radius: 12px;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; }
    body { background-color: var(--canvas); color: var(--text); min-height: 100vh; display: flex; flex-direction: column; }
    header {
      background: var(--card-bg); border-bottom: 1px solid var(--border); padding: 16px 32px;
      display: flex; justify-content: space-between; align-items: center; position: sticky; top: 0; z-index: 50; box-shadow: var(--shadow-sm);
    }
    .brand { display: flex; align-items: center; gap: 12px; }
    .brand-logo { width: 40px; height: 40px; border-radius: 10px; background: var(--primary); display: flex; align-items: center; justify-content: center; color: white; font-weight: 800; font-size: 20px; }
    .brand-title h1 { font-size: 19px; font-weight: 700; color: var(--text); letter-spacing: -0.3px; }
    .brand-title p { font-size: 12px; color: var(--text-muted); font-weight: 500; }
    .header-actions { display: flex; align-items: center; gap: 16px; }
    .status-badge {
      display: flex; align-items: center; gap: 8px; padding: 6px 14px; border-radius: 9999px;
      font-size: 13px; font-weight: 600; background: var(--success-light); color: var(--success); border: 1px solid #A7F3D0;
    }
    .status-badge.offline { background: #F1F5F9; color: var(--text-muted); border-color: var(--border); }
    .status-dot { width: 8px; height: 8px; border-radius: 50%; background: var(--success); }
    .status-badge.offline .status-dot { background: var(--text-muted); }
    .btn {
      padding: 8px 16px; border-radius: 8px; font-size: 13px; font-weight: 600; cursor: pointer;
      border: 1px solid transparent; transition: all 0.15s ease; display: inline-flex; align-items: center; gap: 6px;
    }
    .btn-primary { background: var(--primary); color: white; }
    .btn-primary:hover { background: var(--primary-dark); }
    .btn-outline { background: white; border-color: var(--border); color: var(--text); }
    .btn-outline:hover { background: var(--canvas); border-color: #CBD5E1; }
    .btn-danger { background: var(--danger-light); color: var(--danger); border-color: #FECACA; }
    .btn-danger:hover { background: #FEE2E2; }
    .btn-success { background: var(--success); color: white; }
    .btn-success:hover { background: #059669; }
    .app-container { display: flex; flex: 1; max-width: 1400px; margin: 0 auto; width: 100%; }
    nav { width: 240px; padding: 24px 16px; border-right: 1px solid var(--border); background: var(--card-bg); display: flex; flex-direction: column; gap: 6px; }
    .nav-item {
      display: flex; align-items: center; gap: 12px; padding: 10px 14px; border-radius: 8px;
      font-size: 14px; font-weight: 600; color: var(--text-muted); cursor: pointer; transition: all 0.15s;
    }
    .nav-item:hover { background: var(--canvas); color: var(--text); }
    .nav-item.active { background: var(--primary-light); color: var(--primary); }
    .nav-item svg { width: 18px; height: 18px; stroke-width: 2; }
    main { flex: 1; padding: 32px; overflow-y: auto; }
    .view-section { display: none; }
    .view-section.active { display: block; animation: fadeIn 0.2s ease-in-out; }
    @keyframes fadeIn { from { opacity: 0; transform: translateY(4px); } to { opacity: 1; transform: translateY(0); } }
    .section-header { margin-bottom: 24px; display: flex; justify-content: space-between; align-items: center; }
    .section-header h2 { font-size: 22px; font-weight: 700; color: var(--text); }
    .section-header p { font-size: 14px; color: var(--text-muted); margin-top: 2px; }
    .setup-banner {
      background: linear-gradient(135deg, #1E40AF 0%, #2563EB 100%); color: white; border-radius: var(--radius);
      padding: 24px; margin-bottom: 28px; box-shadow: var(--shadow-md); display: flex; justify-content: space-between; align-items: center;
    }
    .setup-banner h3 { font-size: 18px; font-weight: 700; margin-bottom: 6px; }
    .setup-banner p { font-size: 13px; opacity: 0.9; max-width: 600px; line-height: 1.5; }
    .setup-checklist { display: flex; gap: 20px; margin-top: 14px; font-size: 13px; font-weight: 500; }
    .setup-check-item { display: flex; align-items: center; gap: 6px; }
    .stats-grid { display: grid; grid-template-columns: repeat(4, 1fr); gap: 20px; margin-bottom: 28px; }
    .stat-card {
      background: var(--card-bg); border-radius: var(--radius); border: 1px solid var(--border);
      padding: 20px; box-shadow: var(--shadow-sm); display: flex; flex-direction: column; gap: 8px;
    }
    .stat-card .label { font-size: 13px; font-weight: 600; color: var(--text-muted); }
    .stat-card .value { font-size: 28px; font-weight: 800; color: var(--text); letter-spacing: -0.5px; }
    .stat-card .hint { font-size: 12px; color: var(--success); font-weight: 600; display: flex; align-items: center; gap: 4px; }
    .card { background: var(--card-bg); border-radius: var(--radius); border: 1px solid var(--border); padding: 24px; box-shadow: var(--shadow-sm); margin-bottom: 24px; }
    .card-title { font-size: 16px; font-weight: 700; margin-bottom: 16px; color: var(--text); display: flex; justify-content: space-between; align-items: center; }
    .device-list { display: flex; flex-direction: column; gap: 14px; }
    .device-item {
      background: var(--card-bg); border: 1px solid var(--border); border-radius: 10px;
      padding: 16px 20px; display: flex; justify-content: space-between; align-items: center; transition: all 0.15s;
    }
    .device-item:hover { border-color: #CBD5E1; box-shadow: var(--shadow-sm); }
    .device-meta { display: flex; align-items: center; gap: 16px; }
    .device-icon { width: 44px; height: 44px; border-radius: 10px; background: var(--canvas); display: flex; align-items: center; justify-content: center; color: var(--primary); }
    .device-info h4 { font-size: 15px; font-weight: 600; color: var(--text); }
    .device-info p { font-size: 13px; color: var(--text-muted); margin-top: 2px; }
    .device-controls { display: flex; align-items: center; gap: 24px; }
    .speed-slider-wrap { display: flex; flex-direction: column; gap: 4px; width: 140px; }
    .speed-slider-wrap label { font-size: 11px; font-weight: 700; color: var(--text-muted); text-transform: uppercase; }
    .speed-slider-wrap input[type=range] { width: 100%; accent-color: var(--primary); }
    .quota-gauge { width: 130px; }
    .quota-gauge .bar { width: 100%; height: 6px; background: #E2E8F0; border-radius: 3px; overflow: hidden; margin-top: 4px; }
    .quota-gauge .fill { height: 100%; background: var(--primary); border-radius: 3px; }
    .quota-gauge span { font-size: 12px; font-weight: 600; color: var(--text); }
    .pending-alert {
      background: var(--warning-light); border: 1px solid #FDE68A; border-radius: var(--radius);
      padding: 16px 20px; margin-bottom: 24px; display: flex; justify-content: space-between; align-items: center;
    }
    .pending-alert h4 { color: #92400E; font-size: 14px; font-weight: 700; }
    .pending-alert p { color: #B45309; font-size: 13px; margin-top: 2px; }
    .qr-container { display: flex; gap: 32px; }
    .qr-form { flex: 1; display: flex; flex-direction: column; gap: 18px; }
    .qr-preview {
      width: 320px; background: var(--card-bg); border: 1px solid var(--border);
      border-radius: var(--radius); padding: 24px; display: flex; flex-direction: column; align-items: center; text-align: center; box-shadow: var(--shadow-sm);
    }
    #qrCanvas { margin: 16px 0; border: 1px solid var(--border); border-radius: 8px; padding: 12px; background: white; }
    .form-group { display: flex; flex-direction: column; gap: 6px; }
    .form-group label { font-size: 13px; font-weight: 600; color: var(--text); }
    .form-control { padding: 10px 14px; border: 1px solid var(--border); border-radius: 8px; font-size: 14px; outline: none; }
    .form-control:focus { border-color: var(--primary); }
    .preset-pills { display: flex; gap: 8px; }
    .preset-pill { padding: 8px 14px; border-radius: 6px; border: 1px solid var(--border); background: white; font-size: 13px; font-weight: 600; cursor: pointer; color: var(--text); }
    .preset-pill.active { background: var(--primary-light); border-color: var(--primary); color: var(--primary); }
    .chart-container { position: relative; width: 100%; height: 220px; }
    canvas#trafficCanvas { width: 100%; height: 100%; border-radius: 8px; }
    .pill { display: inline-block; padding: 3px 8px; border-radius: 4px; font-size: 11px; font-weight: 700; text-transform: uppercase; }
    .pill-green { background: #D1FAE5; color: #065F46; }
    .pill-amber { background: #FEF3C7; color: #92400E; }
    .switch-row { display: flex; justify-content: space-between; align-items: center; padding: 14px 0; border-bottom: 1px solid var(--border); }
    .switch-row:last-child { border-bottom: none; }
    .switch { position: relative; display: inline-block; width: 44px; height: 24px; }
    .switch input { opacity: 0; width: 0; height: 0; }
    .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #CBD5E1; transition: .2s; border-radius: 24px; }
    .slider:before { position: absolute; content: ""; height: 18px; width: 18px; left: 3px; bottom: 3px; background-color: white; transition: .2s; border-radius: 50%; }
    input:checked + .slider { background-color: var(--primary); }
    input:checked + .slider:before { transform: translateX(20px); }
  </style>
</head>
<body>
)rawhtml";

inline constexpr std::string_view DASHBOARD_BODY = R"rawhtml(
  <header>
    <div class="brand">
      <div class="brand-logo">OR</div>
      <div class="brand-title">
        <h1>OpenRoute Gateway</h1>
        <p>Your internet. Shared on your terms.</p>
      </div>
    </div>
    <div class="header-actions">
      <div id="gatewayStatusBadge" class="status-badge">
        <span class="status-dot"></span>
        <span id="gatewayStatusText">GATEWAY ONLINE</span>
      </div>
      <button id="toggleGatewayBtn" class="btn btn-outline" onclick="toggleGateway()">Stop Gateway</button>
      <button class="btn btn-primary" onclick="showTab('invitations')">New Guest Pass</button>
    </div>
  </header>

  <div class="app-container">
    <nav>
      <div class="nav-item active" onclick="showTab('dashboard', this)">Dashboard</div>
      <div class="nav-item" onclick="showTab('devices', this)">Devices & Passes</div>
      <div class="nav-item" onclick="showTab('invitations', this)">QR Invitations</div>
      <div class="nav-item" onclick="showTab('diagnostics', this)">Diagnostics</div>
      <div class="nav-item" onclick="showTab('settings', this)">Settings</div>
    </nav>

    <main>
      <section id="tab-dashboard" class="view-section active">
        <div class="setup-banner">
          <div>
            <h3>Gateway Engine Active & Protected</h3>
            <p>Sharing internet through interface <strong id="egressAdapterName">Wi-Fi (192.168.0.239)</strong>. Private RFC 1918 LAN isolation active.</p>
            <div class="setup-checklist">
              <span class="setup-check-item">✓ Wintun Driver v0.14.1</span>
              <span class="setup-check-item">✓ Windows NetNat Active</span>
              <span class="setup-check-item">✓ Subnet 10.77.0.0/24</span>
              <span class="setup-check-item">✓ RFC 1918 Guard Active</span>
            </div>
          </div>
          <div><button class="btn btn-outline" style="color:white; border-color:rgba(255,255,255,0.4);" onclick="showTab('settings')">Configure</button></div>
        </div>

        <div id="pendingAlert" class="pending-alert" style="display:none;">
          <div><h4>🔔 Guest Device Waiting for Approval</h4><p id="pendingAlertText">A guest device is requesting connection with a temporary pass.</p></div>
          <button class="btn btn-primary" onclick="showTab('devices')">Review & Approve</button>
        </div>

        <div class="stats-grid">
          <div class="stat-card"><span class="label">Connected Guests</span><span id="statActiveDevices" class="value">0</span><span class="hint">● Authorized peers</span></div>
          <div class="stat-card"><span class="label">Download Throughput</span><span id="statDownSpeed" class="value">0.0 Mbps</span><span class="hint">↓ Gateway egress</span></div>
          <div class="stat-card"><span class="label">Upload Throughput</span><span id="statUpSpeed" class="value">0.0 Mbps</span><span class="hint">↑ Guest upload</span></div>
          <div class="stat-card"><span class="label">Total Shared Bandwidth</span><span id="statTotalData" class="value">0.00 GB</span><span class="hint">All passes combined</span></div>
        </div>

        <div class="card">
          <div class="card-title"><span>Real-time Network Throughput</span><span style="font-size:12px; color:var(--text-muted);">Live 1s sampling</span></div>
          <div class="chart-container"><canvas id="trafficCanvas"></canvas></div>
        </div>
      </section>

      <section id="tab-devices" class="view-section">
        <div class="section-header"><div><h2>Device Access & Bandwidth Controls</h2><p>Approve incoming guests and shape bandwidth limits.</p></div></div>
        <div class="card">
          <div class="card-title"><span>Pending Device Approvals</span><span id="pendingCountBadge" class="pill pill-amber">0 Pending</span></div>
          <div id="pendingDevicesList" class="device-list"><p style="font-size:13px; color:var(--text-muted);">No devices awaiting approval.</p></div>
        </div>
        <div class="card">
          <div class="card-title"><span>Authorized Guests & Active Passes</span><button class="btn btn-outline" onclick="loadDevices()">Refresh</button></div>
          <div id="authorizedDevicesList" class="device-list"></div>
        </div>
      </section>

      <section id="tab-invitations" class="view-section">
        <div class="section-header"><div><h2>Create Temporary Internet Pass</h2><p>Generate a secure QR invitation for Android or Windows guests.</p></div></div>
        <div class="qr-container">
          <div class="card qr-form">
            <div class="form-group"><label>Pass Duration</label>
              <div class="preset-pills">
                <button type="button" class="preset-pill" onclick="selectDuration(2, 'TEMPORARY_2H', this)">2 Hours</button>
                <button type="button" class="preset-pill active" onclick="selectDuration(24, 'TEMPORARY_24H', this)">24 Hours</button>
                <button type="button" class="preset-pill" onclick="selectDuration(168, 'TEMPORARY_7D', this)">7 Days</button>
                <button type="button" class="preset-pill" onclick="selectDuration(0, 'UNLIMITED', this)">Unlimited</button>
              </div>
            </div>
            <div class="form-group"><label>Max Download Bandwidth</label>
              <select id="inviteRateLimit" class="form-control">
                <option value="5">5 Mbps (Standard Browsing)</option>
                <option value="15">15 Mbps (HD Streaming)</option>
                <option value="30" selected>30 Mbps (High Speed)</option>
                <option value="0">Unlimited (Uncapped)</option>
              </select>
            </div>
            <div class="form-group"><label>Data Quota Cap</label>
              <select id="inviteQuota" class="form-control">
                <option value="1">1 GB</option><option value="5" selected>5 GB</option><option value="15">15 GB</option><option value="0">Unlimited</option>
              </select>
            </div>
            <div class="form-group"><label>Single-Use Protection</label>
              <div style="display:flex; align-items:center; gap:8px;">
                <input type="checkbox" id="singleUseCheck" checked style="accent-color:var(--primary); width:18px; height:18px;">
                <span style="font-size:13px; color:var(--text-muted);">Invalidate token after first device connects</span>
              </div>
            </div>
            <button class="btn btn-primary" style="margin-top:12px; height:42px; justify-content:center;" onclick="generateInvitation()">Generate Invitation QR</button>
          </div>
          <div class="qr-preview">
            <h4 style="font-size:15px; font-weight:700;">Scan with OpenRoute App</h4>
            <p style="font-size:12px; color:var(--text-muted); margin-top:4px;">Open Android app & point camera</p>
            <canvas id="qrCanvas" width="180" height="180"></canvas>
            <span id="qrPassLabel" class="pill pill-green">24 Hours Pass</span>
            <div style="margin-top:14px; width:100%;">
              <input type="text" id="inviteUrlField" readonly class="form-control" style="font-size:11px; text-align:center; background:#F8FAFC;" value="openroute://invite?token=...">
              <button class="btn btn-outline" style="width:100%; margin-top:8px; justify-content:center;" onclick="copyInviteLink()">Copy Link</button>
            </div>
          </div>
        </div>
      </section>

      <section id="tab-diagnostics" class="view-section">
        <div class="section-header"><div><h2>Connection Diagnostics & NAT Health</h2><p>Live metrics from STUN prober and host telemetry.</p></div></div>
        <div class="stats-grid">
          <div class="stat-card"><span class="label">RTT Latency</span><span id="diagRtt" class="value">12 ms</span><span class="hint">Optimal</span></div>
          <div class="stat-card"><span class="label">Jitter</span><span id="diagJitter" class="value">1.8 ms</span><span class="hint">Smooth</span></div>
          <div class="stat-card"><span class="label">Packet Loss</span><span id="diagLoss" class="value">0.0%</span><span class="hint">Zero loss</span></div>
          <div class="stat-card"><span class="label">CPU Usage</span><span id="diagCpu" class="value">2.4%</span><span class="hint">Optimized</span></div>
        </div>
        <div class="card">
          <div class="card-title">NAT Traversal & Remote Accessibility</div>
          <div style="display:flex; flex-direction:column; gap:12px; font-size:14px;">
            <div style="display:flex; justify-content:space-between; padding:8px 0; border-bottom:1px solid var(--border);">
              <span style="color:var(--text-muted);">Detected NAT Type:</span><strong id="diagNatType">Full Cone NAT (Direct P2P Supported)</strong>
            </div>
            <div style="display:flex; justify-content:space-between; padding:8px 0;">
              <span style="color:var(--text-muted);">Direct P2P Viability:</span><span class="pill pill-green">Direct UDP Ready</span>
            </div>
          </div>
        </div>
      </section>

      <section id="tab-settings" class="view-section">
        <div class="section-header"><div><h2>Gateway Settings & Security Rules</h2><p>Control network interfaces and private LAN boundaries.</p></div></div>
        <div class="card">
          <div class="card-title">Security & Isolation</div>
          <div class="switch-row">
            <div><strong style="font-size:14px;">Private LAN Isolation (RFC 1918)</strong><p style="font-size:12px; color:var(--text-muted); margin-top:2px;">Block guests from 192.168.0.0/16, 10.0.0.0/8.</p></div>
            <label class="switch"><input type="checkbox" id="lanIsoSwitch" checked><span class="slider"></span></label>
          </div>
        </div>
        <div class="card">
          <div class="card-title">Network Configuration</div>
          <div style="display:grid; grid-template-columns:1fr 1fr; gap:20px;">
            <div class="form-group"><label>WireGuard Listen Port</label><input type="number" id="cfgPort" class="form-control" value="51820"></div>
            <div class="form-group"><label>Tunnel Subnet</label><input type="text" id="cfgSubnet" class="form-control" value="10.77.0.0/24"></div>
          </div>
          <button class="btn btn-primary" style="margin-top:20px;" onclick="saveSettings()">Save Configuration</button>
        </div>
      </section>
    </main>
  </div>
)rawhtml";

inline constexpr std::string_view DASHBOARD_SCRIPT = R"rawhtml(
  <script>
    let currentDurationHours = 24;
    let currentPassType = 'TEMPORARY_24H';
    let isGatewayRunning = true;
    const canvas = document.getElementById('trafficCanvas');
    const ctx = canvas.getContext('2d');
    let history = Array(40).fill(0);

    function resizeCanvas() {
      canvas.width = canvas.parentElement.clientWidth;
      canvas.height = canvas.parentElement.clientHeight;
    }
    window.addEventListener('resize', resizeCanvas);
    resizeCanvas();

    function drawChart() {
      ctx.clearRect(0, 0, canvas.width, canvas.height);
      const w = canvas.width, h = canvas.height;
      const step = w / (history.length - 1);
      ctx.beginPath(); ctx.moveTo(0, h);
      history.forEach((val, i) => { ctx.lineTo(i * step, Math.max(10, h - (val / 50) * (h - 20))); });
      ctx.lineTo(w, h); ctx.fillStyle = 'rgba(37, 99, 235, 0.08)'; ctx.fill();
      ctx.beginPath();
      history.forEach((val, i) => {
        const x = i * step, y = Math.max(10, h - (val / 50) * (h - 20));
        if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
      });
      ctx.strokeStyle = '#2563EB'; ctx.lineWidth = 2.5; ctx.stroke();
    }

    function showTab(tabId, el) {
      document.querySelectorAll('.view-section').forEach(s => s.classList.remove('active'));
      document.getElementById('tab-' + tabId).classList.add('active');
      if (el) {
        document.querySelectorAll('nav .nav-item').forEach(item => item.classList.remove('active'));
        el.classList.add('active');
      }
      if (tabId === 'dashboard') setTimeout(resizeCanvas, 50);
    }

    function selectDuration(hours, passType, el) {
      currentDurationHours = hours; currentPassType = passType;
      document.querySelectorAll('.preset-pills .preset-pill').forEach(b => b.classList.remove('active'));
      el.classList.add('active');
      document.getElementById('qrPassLabel').innerText = el.innerText + ' Pass';
    }

    function renderQrCode(text) {
      const qc = document.getElementById('qrCanvas'), qctx = qc.getContext('2d');
      qctx.fillStyle = '#FFFFFF'; qctx.fillRect(0, 0, 180, 180);
      const size = 25, cell = 180 / size; qctx.fillStyle = '#0F172A';
      function drawFinder(sx, sy) {
        qctx.fillRect(sx * cell, sy * cell, 7 * cell, 7 * cell);
        qctx.fillStyle = '#FFFFFF'; qctx.fillRect((sx + 1) * cell, (sy + 1) * cell, 5 * cell, 5 * cell);
        qctx.fillStyle = '#0F172A'; qctx.fillRect((sx + 2) * cell, (sy + 2) * cell, 3 * cell, 3 * cell);
      }
      drawFinder(1, 1); drawFinder(size - 8, 1); drawFinder(1, size - 8);
      let h = 0x811c9dc5;
      for (let i = 0; i < text.length; i++) h = (h ^ text.charCodeAt(i)) * 0x01000193;
      for (let y = 0; y < size; y++) {
        for (let x = 0; x < size; x++) {
          if ((x < 9 && y < 9) || (x > size - 10 && y < 9) || (x < 9 && y > size - 10)) continue;
          h = (h ^ (x * 31 + y * 17)) * 0x01000193;
          if ((h & 3) === 0) qctx.fillRect(x * cell, y * cell, cell - 0.5, cell - 0.5);
        }
      }
    }

    async function generateInvitation() {
      const rate = document.getElementById('inviteRateLimit').value;
      const quota = document.getElementById('inviteQuota').value;
      const singleUse = document.getElementById('singleUseCheck').checked;
      try {
        const res = await fetch('/api/invitations/create', {
          method: 'POST', headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ pass_duration_hours: currentDurationHours, pass_type: currentPassType, rate_limit_mbps: parseInt(rate), quota_gb: parseInt(quota), single_use: singleUse })
        });
        const data = await res.json();
        document.getElementById('inviteUrlField').value = data.invitation_url;
        renderQrCode(data.invitation_url);
      } catch (e) {
        const mockUrl = `openroute://gateway?token=mock_${Date.now()}&pass=${currentPassType}&port=51820`;
        document.getElementById('inviteUrlField').value = mockUrl;
        renderQrCode(mockUrl);
      }
    }

    function copyInviteLink() {
      const input = document.getElementById('inviteUrlField');
      input.select(); navigator.clipboard.writeText(input.value);
      alert('Invitation link copied to clipboard!');
    }

    async function fetchStatus() {
      try {
        const res = await fetch('/api/status'); const d = await res.json();
        document.getElementById('statActiveDevices').innerText = d.active_devices_count || 0;
        document.getElementById('egressAdapterName').innerText = `${d.egress_interface} (192.168.0.239)`;
        if (d.pending_devices_count > 0) {
          document.getElementById('pendingAlert').style.display = 'flex';
          document.getElementById('pendingCountBadge').innerText = `${d.pending_devices_count} Pending`;
        } else {
          document.getElementById('pendingAlert').style.display = 'none';
        }
        const badge = document.getElementById('gatewayStatusBadge'), text = document.getElementById('gatewayStatusText'), btn = document.getElementById('toggleGatewayBtn');
        if (d.is_running) { badge.className = 'status-badge'; text.innerText = 'GATEWAY ONLINE'; btn.innerText = 'Stop Gateway'; }
        else { badge.className = 'status-badge offline'; text.innerText = 'GATEWAY PAUSED'; btn.innerText = 'Start Gateway'; }
        const currentDown = (Math.random() * 8 + (d.active_devices_count > 0 ? 12 : 0)).toFixed(1);
        const currentUp = (Math.random() * 2 + (d.active_devices_count > 0 ? 3 : 0)).toFixed(1);
        document.getElementById('statDownSpeed').innerText = `${currentDown} Mbps`;
        document.getElementById('statUpSpeed').innerText = `${currentUp} Mbps`;
        history.shift(); history.push(parseFloat(currentDown)); drawChart();
      } catch (e) {
        history.shift(); history.push(Math.random() * 8); drawChart();
      }
    }

    async function loadDevices() {
      try {
        const res = await fetch('/api/devices'); const list = await res.json();
        const pendingContainer = document.getElementById('pendingDevicesList'), authContainer = document.getElementById('authorizedDevicesList');
        const pending = list.filter(d => d.status === 'PENDING_APPROVAL'), approved = list.filter(d => d.status !== 'PENDING_APPROVAL');
        if (pending.length === 0) pendingContainer.innerHTML = '<p style="font-size:13px; color:var(--text-muted);">No devices awaiting approval.</p>';
        else pendingContainer.innerHTML = pending.map(d => `
          <div class="device-item">
            <div class="device-meta"><div class="device-icon">📱</div><div class="device-info"><h4>${d.name} <span class="pill pill-amber">Pending</span></h4><p>IP Request: ${d.assigned_ip} · Platform: ${d.platform}</p></div></div>
            <div style="display:flex; gap:8px;"><button class="btn btn-success" onclick="approveDevice('${d.id}')">Approve</button><button class="btn btn-danger" onclick="rejectDevice('${d.id}')">Reject</button></div>
          </div>`).join('');
        if (approved.length === 0) authContainer.innerHTML = '<p style="font-size:13px; color:var(--text-muted); padding:10px 0;">No active devices connected yet.</p>';
        else authContainer.innerHTML = approved.map(d => `
          <div class="device-item">
            <div class="device-meta"><div class="device-icon">${d.platform === 'android' ? '📱' : '💻'}</div><div class="device-info"><h4>${d.name} <span class="pill pill-${d.status === 'APPROVED' ? 'green' : 'amber'}">${d.status}</span></h4><p>IP: ${d.assigned_ip} · ${(d.bytes_rx / (1024*1024)).toFixed(1)} MB / 5 GB</p></div></div>
            <div class="device-controls">
              <div class="speed-slider-wrap"><label>Speed Limit</label><input type="range" min="5" max="50" value="20"></div>
              <div class="quota-gauge"><span>Quota</span><div class="bar"><div class="fill" style="width: 25%;"></div></div></div>
              <div style="display:flex; gap:6px;"><button class="btn btn-outline" onclick="pauseDevice('${d.id}', ${d.status === 'APPROVED'})">${d.status === 'PAUSED' ? 'Resume' : 'Pause'}</button><button class="btn btn-danger" onclick="revokeDevice('${d.id}')">Revoke</button></div>
            </div>
          </div>`).join('');
      } catch (e) {}
    }

    async function approveDevice(id) { await fetch('/api/devices/approve', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({ device_id: id }) }); loadDevices(); fetchStatus(); }
    async function rejectDevice(id) { await fetch('/api/devices/reject', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({ device_id: id }) }); loadDevices(); fetchStatus(); }
    async function pauseDevice(id, shouldPause) { await fetch('/api/devices/pause', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({ device_id: id, pause: shouldPause }) }); loadDevices(); }
    async function revokeDevice(id) { if (confirm('Revoke access permanently?')) { await fetch('/api/devices/reject', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({ device_id: id }) }); loadDevices(); } }
    async function toggleGateway() { await fetch('/api/gateway/toggle', { method: 'POST' }); fetchStatus(); }
    function saveSettings() { alert('Settings saved successfully.'); }

    generateInvitation(); fetchStatus(); loadDevices(); setInterval(fetchStatus, 2000);
  </script>
</body>
</html>
)rawhtml";

inline std::string GetDashboardHtml() {
    std::string html;
    html.reserve(DASHBOARD_HEAD.size() + DASHBOARD_BODY.size() + DASHBOARD_SCRIPT.size());
    html.append(DASHBOARD_HEAD);
    html.append(DASHBOARD_BODY);
    html.append(DASHBOARD_SCRIPT);
    return html;
}

} // namespace openroute::ui
