#pragma once

#include <string>
#include <string_view>

namespace openroute::ui {

inline constexpr std::string_view DASHBOARD_CSS = R"rawhtml(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>OpenRoute Gateway – Internet Sharing & Monetization Manager</title>
  <style>
    :root {
      --primary: #7c3aed;
      --primary-hover: #6d28d9;
      --primary-light: #ede9fe;
      --primary-border: #ddd6fe;
      --bg-gradient: linear-gradient(135deg, #e0e7ff 0%, #ede9fe 50%, #f5f3ff 100%);
      --card-bg: #ffffff;
      --text-main: #1e1b4b;
      --text-muted: #6b7280;
      --border: #e5e7eb;
      --success: #059669;
      --success-bg: #ecfdf5;
      --success-border: #a7f3d0;
      --danger: #dc2626;
      --danger-bg: #fee2e2;
      --danger-border: #fca5a5;
      --radius-lg: 18px;
      --radius-md: 12px;
      --radius-sm: 8px;
      --shadow: 0 10px 30px rgba(0, 0, 0, 0.05);
    }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
    body { background: var(--bg-gradient); color: var(--text-main); min-height: 100vh; padding: 20px; display: flex; flex-direction: column; }
    .app-container { max-width: 1300px; margin: 0 auto; width: 100%; display: flex; flex-direction: column; gap: 16px; }

    .top-header {
      background: rgba(255, 255, 255, 0.85); backdrop-filter: blur(12px); border: 1px solid rgba(255, 255, 255, 0.6);
      border-radius: var(--radius-lg); padding: 16px 24px; display: flex; justify-content: space-between; align-items: center; box-shadow: var(--shadow);
    }
    .brand-area { display: flex; align-items: center; gap: 14px; }
    .brand-icon {
      width: 44px; height: 44px; border-radius: 12px; background: var(--primary);
      display: flex; align-items: center; justify-content: center; color: white;
    }
    .brand-icon svg { width: 26px; height: 26px; fill: white; }
    .brand-title h1 { font-size: 22px; font-weight: 800; color: var(--text-main); letter-spacing: -0.3px; }
    .brand-title p { font-size: 13px; color: var(--text-muted); font-weight: 500; }

    .header-badges { display: flex; align-items: center; gap: 10px; flex-wrap: wrap; }
    .badge {
      display: inline-flex; align-items: center; gap: 6px; padding: 6px 14px; border-radius: 9999px;
      font-size: 13px; font-weight: 600; background: white; border: 1px solid var(--border); color: #374151; box-shadow: 0 1px 3px rgba(0,0,0,0.03);
    }
    .badge-live { background: var(--success-bg); border-color: var(--success-border); color: var(--success); }
    .pulse-dot { width: 8px; height: 8px; border-radius: 50%; background: var(--success); animation: pulse 1.5s infinite; }
    @keyframes pulse { 0% { opacity: 1; transform: scale(1); } 50% { opacity: 0.4; transform: scale(1.2); } 100% { opacity: 1; transform: scale(1); } }

    .main-layout { display: flex; gap: 20px; align-items: flex-start; }

    .sidebar {
      width: 240px; background: rgba(255, 255, 255, 0.7); backdrop-filter: blur(8px);
      border: 1px solid rgba(255, 255, 255, 0.8); border-radius: var(--radius-lg); padding: 16px;
      display: flex; flex-direction: column; gap: 8px; box-shadow: var(--shadow);
    }
    .nav-btn {
      display: flex; align-items: center; gap: 12px; padding: 12px 16px; border-radius: var(--radius-md);
      font-size: 14px; font-weight: 600; color: #4b5563; background: transparent; border: none; cursor: pointer;
      text-align: left; transition: all 0.2s ease; width: 100%;
    }
    .nav-btn:hover { background: rgba(255, 255, 255, 0.9); color: var(--text-main); }
    .nav-btn.active {
      background: var(--primary); color: white; box-shadow: 0 4px 14px rgba(124, 58, 237, 0.35);
    }
    .nav-btn svg { width: 18px; height: 18px; fill: currentColor; }

    .content-area { flex: 1; display: flex; flex-direction: column; gap: 14px; }

    .tabs-header { display: flex; gap: 10px; }
    .tab-pill {
      display: inline-flex; align-items: center; gap: 8px; padding: 10px 20px; border-radius: 9999px;
      font-size: 14px; font-weight: 600; cursor: pointer; border: 1px solid var(--border);
      background: rgba(255, 255, 255, 0.9); color: #4b5563; transition: all 0.2s;
    }
    .tab-pill:hover { background: white; color: var(--text-main); }
    .tab-pill.active {
      background: var(--primary); color: white; border-color: var(--primary); box-shadow: 0 4px 12px rgba(124, 58, 237, 0.3);
    }

    .card {
      background: var(--card-bg); border-radius: var(--radius-lg); padding: 24px;
      border: 1px solid var(--border); box-shadow: var(--shadow);
    }

    .table-container { overflow-x: auto; margin-bottom: 20px; border-radius: var(--radius-md); border: 1px solid var(--border); }
    table { width: 100%; border-collapse: collapse; text-align: left; }
    thead tr { background: #f9fafb; border-bottom: 1px solid var(--border); }
    th { padding: 14px 16px; font-size: 13px; font-weight: 700; color: #4b5563; text-transform: uppercase; letter-spacing: 0.5px; }
    tbody tr {
      border-bottom: 1px solid #f3f4f6; cursor: pointer; transition: background 0.15s ease;
    }
    tbody tr:hover { background: #fdfcff; }
    tbody tr.selected {
      background: #ede9fe !important; outline: 2px solid var(--primary);
    }
    td { padding: 14px 16px; font-size: 14px; color: #374151; vertical-align: middle; }

    .status-pill {
      display: inline-flex; padding: 4px 10px; border-radius: 20px; font-size: 11px; font-weight: 700; letter-spacing: 0.5px;
    }
    .status-active { background: #dcfce7; color: #166534; border: 1px solid #bbf7d0; }
    .status-paused { background: #fef3c7; color: #92400e; border: 1px solid #fde68a; }
    .status-hungup { background: #fee2e2; color: #991b1b; border: 1px solid #fecaca; }
    .status-pending { background: #e0e7ff; color: #3730a3; border: 1px solid #c7d2fe; }

    .action-bar { display: flex; align-items: center; gap: 12px; }
    .btn {
      padding: 10px 22px; border-radius: 10px; font-size: 14px; font-weight: 600; cursor: pointer;
      display: inline-flex; align-items: center; gap: 8px; transition: all 0.2s ease; border: 1px solid transparent;
    }
    .btn:active { transform: scale(0.98); }
    .btn-approve { background: var(--primary); color: white; box-shadow: 0 4px 12px rgba(124, 58, 237, 0.3); }
    .btn-approve:hover { background: var(--primary-hover); }
    .btn-pause { background: white; border-color: #d1d5db; color: #374151; }
    .btn-pause:hover { background: #f9fafb; border-color: #9ca3af; }
    .btn-hangup { background: var(--danger-bg); border-color: var(--danger-border); color: var(--danger); }
    .btn-hangup:hover { background: #fecaca; }
    .btn-refresh { margin-left: auto; background: white; border-color: #d1d5db; color: #374151; }
    .btn-refresh:hover { background: #f9fafb; }

    .grid-2col { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; }
    .subcard {
      background: white; border-radius: var(--radius-md); padding: 20px; border: 1px solid var(--border);
      display: flex; flex-direction: column; gap: 16px;
    }
    .subcard-title { font-size: 16px; font-weight: 700; color: var(--text-main); display: flex; align-items: center; gap: 8px; }
    .form-group { display: flex; flex-direction: column; gap: 6px; }
    .form-group label { font-size: 13px; font-weight: 600; color: #4b5563; }
    .form-control {
      padding: 10px 14px; border: 1px solid var(--border); border-radius: var(--radius-sm); font-size: 14px;
      outline: none; transition: border-color 0.2s; background: #f9fafb;
    }
    .form-control:focus { border-color: var(--primary); background: white; }

    .pin-display-box {
      background: #f5f3ff; border: 2px dashed var(--primary-border); border-radius: var(--radius-md);
      padding: 18px; text-align: center; color: #5b21b6; font-size: 26px; font-weight: 800; letter-spacing: 2px;
    }

    .guide-banner {
      background: #f5f3ff; border: 1px solid var(--primary-border); border-radius: var(--radius-md);
      padding: 16px 20px; display: flex; justify-content: space-between; align-items: center;
    }
    .guide-step {
      background: #f9fafb; border: 1px solid var(--border); border-radius: var(--radius-md);
      padding: 16px 20px; margin-top: 12px;
    }
    .guide-step h3 { font-size: 15px; font-weight: 700; color: var(--text-main); margin-bottom: 8px; display: flex; align-items: center; gap: 8px; }
    .guide-step ol { padding-left: 20px; font-size: 13.5px; color: #4b5563; line-height: 1.6; }

    .toast {
      position: fixed; bottom: 24px; right: 24px; background: #1e1b4b; color: white; padding: 12px 20px;
      border-radius: var(--radius-sm); font-size: 14px; font-weight: 600; box-shadow: 0 10px 25px rgba(0,0,0,0.2);
      transform: translateY(100px); opacity: 0; transition: all 0.3s ease; z-index: 9999;
    }
    .toast.show { transform: translateY(0); opacity: 1; }
  </style>
</head>
)rawhtml";

inline constexpr std::string_view DASHBOARD_BODY = R"rawhtml(
<body>
  <div class="app-container">
    <header class="top-header">
      <div class="brand-area">
        <div class="brand-icon">
          <svg viewBox="0 0 24 24"><path d="M12 3C6.95 3 3.15 4.85 0 7.23L12 22 24 7.25C20.85 4.87 17.05 3 12 3zm0 3.82c3.9 0 7.02 1.34 9.4 3.09L12 19.46 2.6 9.91C4.98 8.16 8.1 6.82 12 6.82z"/></svg>
        </div>
        <div class="brand-title">
          <h1>OpenRoute Gateway</h1>
          <p>Internet Sharing & Monetization Manager</p>
        </div>
      </div>

      <div class="header-badges">
        <div class="badge badge-live"><div class="pulse-dot"></div> LIVE - SHARING 5G/WI-FI</div>
        <div class="badge" id="badgePublicIp">&#127760; Public: Discovering...</div>
        <div class="badge" id="badgeActiveCalls">&#128222; Active Calls: 0</div>
        <div class="badge" id="badgeRevenue">&#128176; Revenue: &#8377; 0.00</div>
      </div>
    </header>

    <div class="main-layout">
      <nav class="sidebar">
        <button class="nav-btn active" id="sideBtn0" onclick="switchTab(0)">
          <svg viewBox="0 0 24 24"><path d="M21 2H3c-1.1 0-2 .9-2 2v12c0 1.1.9 2 2 2h7v2H8v2h8v-2h-2v-2h7c1.1 0 2-.9 2-2V4c0-1.1-.9-2-2-2zm0 14H3V4h18v12z"/></svg>
          Live Calls & Devices
        </button>
        <button class="nav-btn" id="sideBtn1" onclick="switchTab(1)">
          <svg viewBox="0 0 24 24"><path d="M11.8 10.9c-2.27-.59-3-1.2-3-2.15 0-1.09 1.01-1.85 2.7-1.85 1.78 0 2.44.85 2.5 2.1h2.21c-.07-1.72-1.12-3.3-3.21-3.81V3h-3v2.16c-1.94.42-3.5 1.68-3.5 3.61 0 2.31 1.91 3.46 4.7 4.13 2.5.6 3 1.48 3 2.41 0 .69-.49 1.79-2.7 1.79-2.06 0-2.87-.92-2.98-2.1h-2.2c.12 2.19 1.76 3.42 3.68 3.83V21h3v-2.15c1.95-.37 3.5-1.5 3.5-3.55 0-2.84-2.43-3.81-4.7-4.4z"/></svg>
          Business & Dial PINs
        </button>
        <button class="nav-btn" id="sideBtn2" onclick="switchTab(2)">
          <svg viewBox="0 0 24 24"><path d="M18 2H6c-1.1 0-2 .9-2 2v16c0 1.1.9 2 2 2h12c1.1 0 2-.9 2-2V4c0-1.1-.9-2-2-2zM6 4h5v8l-2.5-1.5L6 12V4z"/></svg>
          Connection Guide
        </button>
      </nav>

      <main class="content-area">
        <div class="tabs-header">
          <button class="tab-pill active" id="tabPill0" onclick="switchTab(0)">&#128222; Live Calls & Devices</button>
          <button class="tab-pill" id="tabPill1" onclick="switchTab(1)">&#128176; Business & Dial PINs</button>
          <button class="tab-pill" id="tabPill2" onclick="switchTab(2)">&#128214; Universal Connection Guide</button>
        </div>

        <div class="card" id="tabContent0">
          <div class="table-container">
            <table>
              <thead>
                <tr>
                  <th>&#128187; Device / Phone</th>
                  <th>&#128222; Dial PIN</th>
                  <th>&#128241; Platform</th>
                  <th>&#127760; Client IP</th>
                  <th>&#128190; Data Used</th>
                  <th>&#9201; Duration</th>
                  <th>&#8377; Billed Amount</th>
                  <th>&#128246; Status</th>
                </tr>
              </thead>
              <tbody id="callsTableBody">
                <tr><td colspan="8" style="text-align:center; padding:30px; color:var(--text-muted);">Loading active calls...</td></tr>
              </tbody>
            </table>
          </div>

          <div class="action-bar">
            <button class="btn btn-approve" onclick="approveSelected()">&#10003; Approve / Activate</button>
            <button class="btn btn-pause" onclick="pauseSelected()">&#10074;&#10074; Pause Call</button>
            <button class="btn btn-hangup" onclick="hangupSelected()">&#128222; Hang Up / Disconnect</button>
            <button class="btn btn-refresh" onclick="fetchCalls()">&#128260; Refresh List</button>
          </div>
        </div>

        <div class="card" id="tabContent1" style="display:none;">
          <div class="grid-2col">
            <div style="display:flex; flex-direction:column; gap:20px;">
              <div class="subcard">
                <div class="subcard-title">&#128176; Monetization Rates (Your Internet Price)</div>
                <div class="form-group">
                  <label>Currency (e.g. INR, USD):</label>
                  <select class="form-control" id="rateCurrency">
                    <option value="INR">INR (&#8377; Indian Rupee)</option>
                    <option value="USD">USD ($ US Dollar)</option>
                    <option value="EUR">EUR (&euro; Euro)</option>
                  </select>
                </div>
                <div class="form-group">
                  <label>Rate per GB:</label>
                  <input type="number" step="0.5" class="form-control" id="rateGb" value="50.00">
                </div>
                <div class="form-group">
                  <label>Rate per Hour:</label>
                  <input type="number" step="0.5" class="form-control" id="rateHour" value="20.00">
                </div>
                <button class="btn btn-approve" onclick="saveRates()" style="align-self:flex-start;">&#128190; Save Pricing Rates</button>
              </div>

              <div class="subcard">
                <div class="subcard-title">&#128202; Lifetime Earnings Ledger</div>
                <div style="font-size:24px; font-weight:800; color:var(--primary);" id="statRev">&#8377; 0.00</div>
                <div style="font-size:14px; color:#4b5563;">Total Internet Sold: <b id="statBytes">0 B</b></div>
                <div style="font-size:14px; color:#4b5563;">Total Call Sessions: <b id="statSessions">0</b></div>
              </div>
            </div>

            <div class="subcard">
              <div class="subcard-title">&#128273; Generate Numeric Dial PIN Pass</div>
              <div class="form-group">
                <label>Pass Duration:</label>
                <select class="form-control" id="genDur">
                  <option value="3600">1 Hour</option>
                  <option value="10800">3 Hours</option>
                  <option value="86400" selected>24 Hours (1 Day)</option>
                  <option value="604800">7 Days</option>
                  <option value="0">Unlimited</option>
                </select>
              </div>
              <div class="form-group">
                <label>Data Quota:</label>
                <select class="form-control" id="genQuota">
                  <option value="524288000">500 MB</option>
                  <option value="1073741824">1 GB</option>
                  <option value="2147483648">2 GB</option>
                  <option value="5368709120" selected>5 GB (Recommended)</option>
                  <option value="10737418240">10 GB</option>
                  <option value="0">Unlimited</option>
                </select>
              </div>
              <button class="btn btn-approve" onclick="generatePin()">&#128273; Generate Dial PIN</button>

              <div class="pin-display-box" id="pinDisplayBox">
                DIAL PIN: --- ---
              </div>

              <button class="btn btn-pause" onclick="copyPassMessage()" style="width:100%; justify-content:center;">
                &#128203; Copy Shareable WhatsApp / SMS Pass Message
              </button>
            </div>
          </div>
        </div>

        <div class="card" id="tabContent2" style="display:none;">
          <div class="guide-banner">
            <div>
              <h2 style="font-size:18px; font-weight:800; color:var(--text-main);">&#128214; OPENROUTE UNIVERSAL CONNECTION GUIDE</h2>
              <p style="font-size:13px; color:var(--text-muted); margin-top:2px;">Connect any phone, laptop, or OS to your high-speed 5G/Wi-Fi connection.</p>
            </div>
            <div style="font-size:24px;">&#128241; &#128187; &#128421;</div>
          </div>

          <div class="guide-step" style="background:#f5f3ff; border-color:var(--primary-border);">
            <h3>&#127760; Your Host Connection Addresses</h3>
            <p style="font-size:13.5px; color:#4b5563; line-height:1.6;">
              &bull; <b>Remote / Public Endpoint:</b> <span id="guidePubIp" style="font-weight:700; color:var(--primary);">103.217.214.146:8080</span><br>
              &bull; <b>Local Wi-Fi Endpoint:</b> <span id="guideLocalIp" style="font-weight:700;">192.168.0.239:8080</span><br>
              &bull; <b>Universal Proxy Port:</b> 8080 &nbsp;|&nbsp; <b>WireGuard VPN Port:</b> 51820
            </p>
          </div>

          <div style="background:#ede9fe; border-left:4px solid var(--primary); padding:10px 16px; border-radius:8px; font-weight:700; font-size:14px; color:var(--primary); margin-top:16px;">
            &#9889; METHOD 1: ZERO APPS NEEDED (BUILT INTO EVERY PHONE & OS)
          </div>

          <div class="guide-step">
            <h3>1. APPLE IPHONE / IPAD (iOS)</h3>
            <ol>
              <li>Open <b>Settings</b> &rarr; <b>Wi-Fi</b> &rarr; Tap the <b>(i)</b> icon next to the connected Wi-Fi network.</li>
              <li>Scroll down to <b>Configure Proxy</b> &rarr; Select <b>Manual</b>.</li>
              <li><b>Server:</b> <span class="host-ip-text">103.217.214.146</span> &nbsp;|&nbsp; <b>Port:</b> 8080</li>
              <li>Turn <b>Authentication</b> ON &rarr; Enter your Dial PIN as password.</li>
              <li>Tap <b>Save</b>. Your entire iPhone now routes internet through the host PC!</li>
            </ol>
          </div>

          <div class="guide-step">
            <h3>2. ANDROID PHONES</h3>
            <ol>
              <li>Open <b>Settings</b> &rarr; <b>Network & Internet</b> &rarr; <b>Wi-Fi</b> &rarr; Tap current network &rarr; <b>Advanced Options</b>.</li>
              <li>Select <b>Proxy</b> &rarr; <b>Manual</b> &rarr; Proxy Hostname: <span class="host-ip-text">103.217.214.146</span>, Port: 8080.</li>
            </ol>
          </div>

          <div class="guide-step">
            <h3>3. WINDOWS / MAC LAPTOPS</h3>
            <ol>
              <li>Windows: <b>Settings</b> &rarr; <b>Network & Internet</b> &rarr; <b>Proxy</b> &rarr; Set Proxy Server to <span class="host-ip-text">103.217.214.146:8080</span>.</li>
              <li>Mac: <b>System Settings</b> &rarr; <b>Network</b> &rarr; <b>Proxies</b> &rarr; Web Proxy: <span class="host-ip-text">103.217.214.146:8080</span>.</li>
            </ol>
          </div>

          <div style="background:#ecfdf5; border-left:4px solid var(--success); padding:10px 16px; border-radius:8px; font-weight:700; font-size:14px; color:var(--success); margin-top:16px;">
            &#128222; METHOD 2: PHONE DIAL-PAD WEB PORTAL ("DIAL TO CONNECT")
          </div>
          <div class="guide-step">
            <p style="font-size:13.5px; color:#4b5563; line-height:1.6;">
              Remote smartphone users can simply open <a href="/dial" target="_blank" style="color:var(--primary); font-weight:700;">http://<span class="host-ip-text">103.217.214.146</span>:8080/dial</a> in their mobile browser, enter their 6-digit Dial PIN on the dial keypad, tap <b>CALL</b> to begin internet routing, and tap <b>HANG UP</b> to disconnect.
            </p>
          </div>
        </div>
      </main>
    </div>
  </div>

  <div class="toast" id="toast">Notification message</div>
)rawhtml";

inline constexpr std::string_view DASHBOARD_SCRIPT = R"rawhtml(
  <script>
    let selectedPin = null;
    let selectedSessionId = null;
    let latestGeneratedPin = '197-141';
    let currentCurrency = 'INR';
    let publicIp = '103.217.214.146';

    function showToast(msg) {
      const t = document.getElementById('toast');
      t.innerText = msg;
      t.classList.add('show');
      setTimeout(() => t.classList.remove('show'), 3000);
    }

    function switchTab(idx) {
      for (let i = 0; i < 3; i++) {
        document.getElementById('tabContent' + i).style.display = (i === idx) ? 'block' : 'none';
        document.getElementById('tabPill' + i).classList.toggle('active', i === idx);
        document.getElementById('sideBtn' + i).classList.toggle('active', i === idx);
      }
      if (idx === 0) fetchCalls();
      if (idx === 1) fetchRatesAndSummary();
    }

    function selectRow(tr, pin, sessionId) {
      document.querySelectorAll('#callsTableBody tr').forEach(r => r.classList.remove('selected'));
      tr.classList.add('selected');
      selectedPin = pin;
      selectedSessionId = sessionId;
    }

    async function fetchCalls() {
      try {
        const resp = await fetch('/api/calls');
        const calls = await resp.json();
        const tbody = document.getElementById('callsTableBody');
        
        if (!calls || calls.length === 0) {
          tbody.innerHTML = '<tr><td colspan="8" style="text-align:center; padding:30px; color:var(--text-muted);">No call sessions or passes generated yet. Click "Business & Dial PINs" to create one.</td></tr>';
          return;
        }

        let html = '';
        calls.forEach(c => {
          const isSelected = (selectedSessionId && c.session_id === selectedSessionId) || (selectedPin && c.dial_pin === selectedPin);
          const selClass = isSelected ? ' selected' : '';
          
          let stClass = 'status-active';
          if (c.status === 'HUNG UP') stClass = 'status-hungup';
          else if (c.status === 'PAUSED') stClass = 'status-paused';
          else if (c.status === 'PENDING') stClass = 'status-pending';

          html += `<tr class="${selClass}" onclick="selectRow(this, '${c.dial_pin}', '${c.session_id}')">
            <td><b>${c.device_name || 'Phone-' + c.dial_pin}</b></td>
            <td><code>${c.dial_pin}</code></td>
            <td>${c.platform || 'Universal'}</td>
            <td>${c.client_ip || 'Pending Connect'}</td>
            <td>${c.data_used || '0 B'}</td>
            <td>${c.duration || '00:00'}</td>
            <td><b>${c.billed || '₹ 0.00'}</b></td>
            <td><span class="status-pill ${stClass}">${c.status}</span></td>
          </tr>`;
        });

        tbody.innerHTML = html;
      } catch (err) {
        console.error('Failed to fetch calls:', err);
      }
    }

    async function fetchRatesAndSummary() {
      try {
        const [ratesResp, sumResp] = await Promise.all([
          fetch('/api/billing/rates'),
          fetch('/api/billing/summary')
        ]);
        const rates = await ratesResp.json();
        const sum = await sumResp.json();

        currentCurrency = rates.currency || 'INR';
        document.getElementById('rateCurrency').value = currentCurrency;
        document.getElementById('rateGb').value = rates.rate_per_gb;
        document.getElementById('rateHour').value = rates.rate_per_hour;

        document.getElementById('statRev').innerText = sum.formatted_revenue || '₹ 0.00';
        document.getElementById('statBytes').innerText = sum.formatted_bytes || '0 B';
        document.getElementById('statSessions').innerText = `${sum.total_sessions || 0} (${sum.active_sessions || 0} Active)`;

        document.getElementById('badgeRevenue').innerHTML = `&#128176; Revenue: ${sum.formatted_revenue || '₹ 0.00'}`;
        document.getElementById('badgeActiveCalls').innerHTML = `&#128222; Active Calls: ${sum.active_sessions || 0}`;

        if (sum.public_ip) {
          publicIp = sum.public_ip;
          document.getElementById('badgePublicIp').innerHTML = `&#127760; Public: ${publicIp}:${sum.public_port || 8080}`;
          document.getElementById('guidePubIp').innerText = `${publicIp}:${sum.public_port || 8080}`;
          document.querySelectorAll('.host-ip-text').forEach(el => el.innerText = publicIp);
        }
        if (sum.local_ip) {
          document.getElementById('guideLocalIp').innerText = `${sum.local_ip}:${sum.public_port || 8080}`;
        }
      } catch (e) {
        console.error(e);
      }
    }

    async function approveSelected() {
      if (!selectedPin && !selectedSessionId) {
        showToast('Please click a device / call row to select it first.');
        return;
      }
      try {
        const resp = await fetch('/api/calls/approve', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ pin: selectedPin, session_id: selectedSessionId })
        });
        const res = await resp.json();
        if (res.success) {
          showToast('Session approved! Remote device now has active internet.');
          fetchCalls();
        } else {
          showToast('Failed to approve session.');
        }
      } catch (e) {
        showToast('Error approving session');
      }
    }

    async function pauseSelected() {
      if (!selectedPin && !selectedSessionId) {
        showToast('Please click a device / call row to select it first.');
        return;
      }
      try {
        const resp = await fetch('/api/calls/pause', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ pin: selectedPin, session_id: selectedSessionId })
        });
        const res = await resp.json();
        if (res.success) {
          showToast(res.paused ? 'Call paused.' : 'Call resumed.');
          fetchCalls();
        } else {
          showToast('Failed to toggle pause.');
        }
      } catch (e) {
        showToast('Error pausing call');
      }
    }

    async function hangupSelected() {
      if (!selectedPin && !selectedSessionId) {
        showToast('Please click a device / call row to select it first.');
        return;
      }
      try {
        const resp = await fetch('/api/calls/hangup', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ pin: selectedPin, session_id: selectedSessionId })
        });
        const res = await resp.json();
        if (res.success) {
          showToast('Call hung up! Internet connection terminated.');
          selectedPin = null;
          selectedSessionId = null;
          fetchCalls();
          fetchRatesAndSummary();
        } else {
          showToast('Failed to hang up call.');
        }
      } catch (e) {
        showToast('Error hanging up call');
      }
    }

    async function saveRates() {
      try {
        const currency = document.getElementById('rateCurrency').value;
        const rate_per_gb = parseFloat(document.getElementById('rateGb').value);
        const rate_per_hour = parseFloat(document.getElementById('rateHour').value);

        const resp = await fetch('/api/billing/rates', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ currency, rate_per_gb, rate_per_hour })
        });
        const res = await resp.json();
        if (res.success) {
          showToast('Pricing rates saved successfully!');
          fetchRatesAndSummary();
        }
      } catch (e) {
        showToast('Failed to save rates');
      }
    }

    async function generatePin() {
      try {
        const duration_sec = parseInt(document.getElementById('genDur').value);
        const quota_bytes = parseInt(document.getElementById('genQuota').value);

        const resp = await fetch('/api/calls/generate-pin', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ duration_sec, quota_bytes, platform: 'Universal' })
        });
        const res = await resp.json();
        if (res.success) {
          latestGeneratedPin = res.formatted_pin;
          document.getElementById('pinDisplayBox').innerText = 'DIAL PIN: ' + res.formatted_pin;
          showToast('Generated Dial PIN: ' + res.formatted_pin);
          fetchCalls();
          fetchRatesAndSummary();
        }
      } catch (e) {
        showToast('Failed to generate PIN');
      }
    }

    function copyPassMessage() {
      const pinRaw = latestGeneratedPin.replace(/[^0-9]/g, '');
      const msg = `=== OPENROUTE 5G/WI-FI INTERNET PASS ===\r\n` +
        `Your Internet Dial PIN: ${pinRaw}\r\n` +
        `Host Address: ${publicIp}:8080\r\n\r\n` +
        `HOW TO CONNECT (ZERO APPS NEEDED):\r\n` +
        `1. iPhone/iPad: Settings -> Wi-Fi -> (i) -> Configure Proxy -> Manual.\r\n` +
        `   Server: ${publicIp} | Port: 8080 | Password: ${pinRaw}\r\n` +
        `2. Android / PC: Set Proxy to ${publicIp}:8080\r\n\r\n` +
        `Or Mobile Phone Dial-Pad:\r\n` +
        `Open http://${publicIp}:8080/dial?pin=${pinRaw} in browser and tap CALL!`;

      navigator.clipboard.writeText(msg).then(() => {
        showToast('Pass message copied to clipboard! Ready to paste on WhatsApp / SMS.');
      }).catch(() => {
        showToast('Copied to clipboard');
      });
    }

    fetchCalls();
    fetchRatesAndSummary();
    setInterval(() => {
      fetchCalls();
      fetchRatesAndSummary();
    }, 2000);
  </script>
</body>
</html>
)rawhtml";

inline std::string GetDashboardHtml() {
    std::string s;
    s.reserve(DASHBOARD_CSS.size() + DASHBOARD_BODY.size() + DASHBOARD_SCRIPT.size());
    s.append(DASHBOARD_CSS);
    s.append(DASHBOARD_BODY);
    s.append(DASHBOARD_SCRIPT);
    return s;
}

} // namespace openroute::ui
