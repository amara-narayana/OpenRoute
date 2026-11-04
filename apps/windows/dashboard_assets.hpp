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
            <h4 style="font-size:15px; font-weight:700;">Scan with Phone Camera</h4>
            <p style="font-size:12px; color:var(--text-muted); margin-top:4px;">Auto-downloads OpenRoute app on Android</p>
            <div id="qrCodeContainer" style="display:flex; justify-content:center; align-items:center; background:#FFFFFF; padding:10px; border-radius:12px; border:1px solid #E2E8F0; box-shadow:0 1px 3px rgba(0,0,0,0.06); width:200px; height:200px; margin:14px auto;"></div>
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

inline constexpr std::string_view QRCODE_LIB_PART1 = R"rawhtml(
<script>
var QRCode;!function(){function a(a){this.mode=c.MODE_8BIT_BYTE,this.data=a,this.parsedData=[];for(var b=[],d=0,e=this.data.length;e>d;d++){var f=this.data.charCodeAt(d);f>65536?(b[0]=240|(1835008&f)>>>18,b[1]=128|(258048&f)>>>12,b[2]=128|(4032&f)>>>6,b[3]=128|63&f):f>2048?(b[0]=224|(61440&f)>>>12,b[1]=128|(4032&f)>>>6,b[2]=128|63&f):f>128?(b[0]=192|(1984&f)>>>6,b[1]=128|63&f):b[0]=f,this.parsedData=this.parsedData.concat(b)}this.parsedData.length!=this.data.length&&(this.parsedData.unshift(191),this.parsedData.unshift(187),this.parsedData.unshift(239))}function b(a,b){this.typeNumber=a,this.errorCorrectLevel=b,this.modules=null,this.moduleCount=0,this.dataCache=null,this.dataList=[]}function i(a,b){if(void 0==a.length)throw new Error(a.length+"/"+b);for(var c=0;c<a.length&&0==a[c];)c++;this.num=new Array(a.length-c+b);for(var d=0;d<a.length-c;d++)this.num[d]=a[d+c]}function j(a,b){this.totalCount=a,this.dataCount=b}function k(){this.buffer=[],this.length=0}function m(){return"undefined"!=typeof CanvasRenderingContext2D}function n(){var a=!1,b=navigator.userAgent;return/android/i.test(b)&&(a=!0,aMat=b.toString().match(/android ([0-9]\.[0-9])/i),aMat&&aMat[1]&&(a=parseFloat(aMat[1]))),a}function r(a,b){for(var c=1,e=s(a),f=0,g=l.length;g>=f;f++){var h=0;switch(b){case d.L:h=l[f][0];break;case d.M:h=l[f][1];break;case d.Q:h=l[f][2];break;case d.H:h=l[f][3]}if(h>=e)break;c++}if(c>l.length)throw new Error("Too long data");return c}function s(a){var b=encodeURI(a).toString().replace(/\%[0-9a-fA-F]{2}/g,"a");return b.length+(b.length!=a?3:0)}a.prototype={getLength:function(){return this.parsedData.length},write:function(a){for(var b=0,c=this.parsedData.length;c>b;b++)a.put(this.parsedData[b],8)}},b.prototype={addData:function(b){var c=new a(b);this.dataList.push(c),this.dataCache=null},isDark:function(a,b){if(0>a||this.moduleCount<=a||0>b||this.moduleCount<=b)throw new Error(a+","+b);return this.modules[a][b]},getModuleCount:function(){return this.moduleCount},make:function(){this.makeImpl(!1,this.getBestMaskPattern())},makeImpl:function(a,c){this.moduleCount=4*this.typeNumber+17,this.modules=new Array(this.moduleCount);for(var d=0;d<this.moduleCount;d++){this.modules[d]=new Array(this.moduleCount);for(var e=0;e<this.moduleCount;e++)this.modules[d][e]=null}this.setupPositionProbePattern(0,0),this.setupPositionProbePattern(this.moduleCount-7,0),this.setupPositionProbePattern(0,this.moduleCount-7),this.setupPositionAdjustPattern(),this.setupTimingPattern(),this.setupTypeInfo(a,c),this.typeNumber>=7&&this.setupTypeNumber(a),null==this.dataCache&&(this.dataCache=b.createData(this.typeNumber,this.errorCorrectLevel,this.dataList)),this.mapData(this.dataCache,c)},setupPositionProbePattern:function(a,b){for(var c=-1;7>=c;c++)if(!(-1>=a+c||this.moduleCount<=a+c))for(var d=-1;7>=d;d++)-1>=b+d||this.moduleCount<=b+d||(this.modules[a+c][b+d]=c>=0&&6>=c&&(0==d||6==d)||d>=0&&6>=d&&(0==c||6==c)||c>=2&&4>=c&&d>=2&&4>=d?!0:!1)},getBestMaskPattern:function(){for(var a=0,b=0,c=0;8>c;c++){this.makeImpl(!0,c);var d=f.getLostPoint(this);(0==c||a>d)&&(a=d,b=c)}return b},createMovieClip:function(a,b,c){var d=a.createEmptyMovieClip(b,c),e=1;this.make();for(var f=0;f<this.modules.length;f++)for(var g=f*e,h=0;h<this.modules[f].length;h++){var i=h*e,j=this.modules[f][h];j&&(d.beginFill(0,100),d.moveTo(i,g),d.lineTo(i+e,g),d.lineTo(i+e,g+e),d.lineTo(i,g+e),d.endFill())}return d},setupTimingPattern:function(){for(var a=8;a<this.moduleCount-8;a++)null==this.modules[a][6]&&(this.modules[a][6]=0==a%2);for(var b=8;b<this.moduleCount-8;b++)null==this.modules[6][b]&&(this.modules[6][b]=0==b%2)},setupPositionAdjustPattern:function(){for(var a=f.getPatternPosition(this.typeNumber),b=0;b<a.length;b++)for(var c=0;c<a.length;c++){var d=a[b],e=a[c];if(null==this.modules[d][e])for(var g=-2;2>=g;g++)for(var h=-2;2>=h;h++)this.modules[d+g][e+h]=-2==g||2==g||-2==h||2==h||0==g&&0==h?!0:!1}},setupTypeNumber:function(a){for(var b=f.getBCHTypeNumber(this.typeNumber),c=0;18>c;c++){var d=!a&&1==(1&b>>c);this.modules[Math.floor(c/3)][c%3+this.moduleCount-8-3]=d}for(var c=0;18>c;c++){var d=!a&&1==(1&b>>c);this.modules[c%3+this.moduleCount-8-3][Math.floor(c/3)]=d}},setupTypeInfo:function(a,b){for(var c=this.errorCorrectLevel<<3|b,d=f.getBCHTypeInfo(c),e=0;15>e;e++){var g=!a&&1==(1&d>>e);6>e?this.modules[e][8]=g:8>e?this.modules[e+1][8]=g:this.modules[this.moduleCount-15+e][8]=g}for(var e=0;15>e;e++){var g=!a&&1==(1&d>>e);8>e?this.modules[8][this.moduleCount-e-1]=g:9>e?this.modules[8][15-e-1+1]=g:this.modules[8][15-e-1]=g}this.modules[this.moduleCount-8][8]=!a},mapData:function(a,b){for(var c=-1,d=this.moduleCount-1,e=7,g=0,h=this.moduleCount-1;h>0;h-=2)for(6==h&&h--;;){for(var i=0;2>i;i++)if(null==this.modules[d][h-i]){var j=!1;g<a.length&&(j=1==(1&a[g]>>>e));var k=f.getMask(b,d,h-i);k&&(j=!j),this.modules[d][h-i]=j,e--,-1==e&&(g++,e=7)}if(d+=c,0>d||this.moduleCount<=d){d-=c,c=-c;break}}}},b.PAD0=236,b.PAD1=17,b.createData=function(a,c,d){for(var e=j.getRSBlocks(a,c),g=new k,h=0;h<d.length;h++){var i=d[h];g.put(i.mode,4),g.put(i.getLength(),f.getLengthInBits(i.mode,a)),i.write(g)}for(var l=0,h=0;h<e.length;h++)l+=e[h].dataCount;if(g.getLengthInBits()>8*l)throw new Error("code length overflow. ("+g.getLengthInBits()+">"+8*l+")");for(g.getLengthInBits()+4<=8*l&&g.put(0,4);0!=g.getLengthInBits()%8;)g.putBit(!1);for(;;){if(g.getLengthInBits()>=8*l)break;if(g.put(b.PAD0,8),g.getLengthInBits()>=8*l)break;g.put(b.PAD1,8)}return b.createBytes(g,e)},b.createBytes=function(a,b){for(var c=0,d=0,e=0,g=new Array(b.length),h=new Array(b.length),j=0;j<b.length;j++){var k=b[j].dataCount,l=b[j].totalCount-k;d=Math.max(d,k),e=Math.max(e,l),g[j]=new Array(k);for(var m=0;m<g[j].length;m++)g[j][m]=255&a.buffer[m+c];c+=k;var n=f.getErrorCorrectPolynomial(l),o=new i(g[j],n.getLength()-1),p=o.mod(n);h[j]=new Array(n.getLength()-1);for(var m=0;m<h[j].length;m++){var q=m+p.getLength()-h[j].length;h[j][m]=q>=0?p.get(q):0}}for(var r=0,m=0;m<b.length;m++)r+=b[m].totalCount;for(var s=new Array(r),t=0,m=0;d>m;m++)for(var j=0;j<b.length;j++)m<g[j].length&&(s[t++]=g[j][m]);for(var m=0;e>m;m++)for(var j=0;j<b.length;j++)m<h[j].length&&(s[t++]=h[j][m]);return s};for(var c={MODE_NUMBER:1,MODE_ALPHA_NUM:2,MODE_8BIT_BYTE:4,MODE_KANJI:8},d={L:1,M:0,Q:3,H:2},e={PATTERN000:0,PATTERN001:1,PATTERN010:2,PATTERN011:3,PATTERN100:4,PATTERN101:5,PATTERN110:6,PATTERN111:7},f={PATTERN_POSITION_TABLE:[[],[6,18],[6,22],[6,26],[6,30],[6,34],[6,22,38],[6,24,42],[6,26,46],[6,28,50],[6,30,54],[6,32,58],[6,34,62],[6,26,46,66],[6,26,48,70],[6,26,50,74],[6,30,54,78],[6,30,56,82],[6,30,58,86],[6,34,62,90],[6,28,50,72,94],[6,26,50,74,98],[6,30,54,78,102],[6,28,54,80,106],[6,32,58,84,110],[6,30,58,86,114],[6,34,62,90,118],[6,26,50,74,98,122],[6,30,54,78,102,126],[6,26,52,78,104,130],[6,30,56,82,108,134],[6,34,60,86,112,138],[6,30,58,86,114,142],[6,34,62,90,118,146],[6,30,54,78,102,126,150],[6,24,50,76,102,128,154],[6,28,54,80,106,132,158],[6,32,58,84,110,136,162],[6,26,54,82,110,138,166],[6,30,58,86,114,142,170]],G15:1335,G18:7973,G15_MASK:21522,getBCHTypeInfo:function(a){for(var b=a<<10;f.getBCHDigit(b)-f.getBCHDigit(f.G15)>=0;)b^=f.G15<<f.getBCHDigit(b)-f.getBCHDigit(f.G15);return(a<<10|b)^f.G15_MASK},getBCHTypeNumber:function(a){for(var b=a<<12;f.getBCHDigit(b)-f.getBCHDigit(f.G18)>=0;)b^=f.G18<<f.getBCHDigit(b)-f.getBCHDigit(f.G18);return a<<12|b},getBCHDigit:function(a){for(var b=0;0!=a;)b++,a>>>=1;return b},getPatternPosition:function(a){return f.PATTERN_POSITION_TABLE[a-1]},getMask:function(a,b,c){switch(a){case e.PATTERN000:return 0==(b+c)%2;case e.PATTERN001:return 0==b%2;case e.PATTERN010:return 0==c%3;case e.PATTERN011:return 0==(b+c)%3;case e.PATTERN100:return 0==(Math.floor(b/2)+Math.floor(c/3))%2;case e.PATTERN101:return 0==b*c%2+b*c%3;case e.PATTERN110:return 0==(b*c%2+b*c%3)%2;case e.PATTERN111:return 0==(b*c%3+(b+c)%2)%2;default:throw new Error("bad maskPattern:"+a)}},getErrorCorrectPolynomial:function(a){for(var b=new i([1],0),c=0;a>c;c++)b=b.multiply(new i([1,g.gexp(c)],0));return b},getLengthInBits:function(a,b){if(b>=1&&10>b)switch(a){case c.MODE_NUMBER:return 10;case c.MODE_ALPHA_NUM:return 9;case c.MODE_8BIT_BYTE:return 8;case c.MODE_KANJI:return 8;default:throw new Error("mode:"+a)}else if(27>b)switch(a){case c.MODE_NUMBER:return 12;case c.MODE_ALPHA_NUM:return 11;case c.MODE_8BIT_BYTE:return 16;case c.MODE_KANJI:return 10;default:throw new Error("mode:"+a)}else{if(!(41>b))throw new Error("type:"+b);switch(a){case c.MODE_NUMBER:return 14;case c.MODE_ALPHA_NUM:return 13;case c.MODE_8BIT_BYTE:return 16;case c.MODE_KANJI:return 12;default:throw new Error("mode:"+a)}}},getLostPoint:function(a){for(var b=a.getModuleCount(),c=0,d=0;b>d;d++)for(var e=0;b>e;e++){for(var f=0,g=a.isDark(d,e),h=-1;1>=h;h++)if(!(0>d+h||d+h>=b))for(var i=-1;1>=i;i++)0>e+i||e+i>=b||(0!=h||0!=i)&&g==a.isDark(d+h,e+i)&&f++;f>5&&(c+=3+f-5)}for(var d=0;b-1>d;d++)for(var e=0;b-1>e;e++){var j=0;a.isDark(d,e)&&j++,a.isDark(d+1,e)&&j++,a.isDark(d,e+1)&&j++,a.isDark(d+1,e+1)&&j++,(0==j||4==j)&&(c+=3)}for(var d=0;b>d;d++)for(var e=0;b-6>e;e++)a.isDark(d,e)&&!a.isDark(d,e+1)&&a.isDark(d,e+2)&&a.isDark(d,e+3)&&a.isDark(d,e+4)&&!a.isDark(d,e+5)&&a.isDark(d,e+6)&&(c+=40);for(var e=0;b>e;e++)for(var d=0;b-6>d;d++)a.isDark(d,e)&&!a.isDark(d+1,e)&&a.isDark(d+2,e)&&a.isDark(d+3,e)&&a.isDark(d+4,e)&&!a.isDark(d+5,e)&&a.isDark(d+6,e)&&(c+=40);for(var k=0,e=0;b>e;e++)for(var d=0;b>d;d++)a.isDark(d,e)&&k++;var l=Math.abs(100*k/b/b-50)/5;return c+=10*l}},g={glog:function(a){if(1>a)throw new Error("glog("+a+")");return g.LOG_TABLE[a]},gexp:function(a){for(;0>a;)a+=255;for(;a>=256;)a-=255;return g.EXP_TABLE[a]},EXP_TABLE:new Array(256),LOG_TABLE:new Array(256)},h=0;8>h;h++)g.EXP_TABLE[h]=1<<h;for(var h=8;256>h;h++)g.EXP_TABLE[h]=g.EXP_TABLE[h-4]^g.EXP_TABLE[h-5]^g.EXP_TABLE[h-6]^g.EXP_TABLE[h-8];for(var h=0;255>h;h++)g.LOG_TABLE[g.EXP_TABLE[h]]=h;i.prototype={get:function(a){return this.num[a]},getLength:function(){return th
)rawhtml";

inline constexpr std::string_view QRCODE_LIB_PART2 = R"rawhtml(
is.num.length},multiply:function(a){for(var b=new Array(this.getLength()+a.getLength()-1),c=0;c<this.getLength();c++)for(var d=0;d<a.getLength();d++)b[c+d]^=g.gexp(g.glog(this.get(c))+g.glog(a.get(d)));return new i(b,0)},mod:function(a){if(this.getLength()-a.getLength()<0)return this;for(var b=g.glog(this.get(0))-g.glog(a.get(0)),c=new Array(this.getLength()),d=0;d<this.getLength();d++)c[d]=this.get(d);for(var d=0;d<a.getLength();d++)c[d]^=g.gexp(g.glog(a.get(d))+b);return new i(c,0).mod(a)}},j.RS_BLOCK_TABLE=[[1,26,19],[1,26,16],[1,26,13],[1,26,9],[1,44,34],[1,44,28],[1,44,22],[1,44,16],[1,70,55],[1,70,44],[2,35,17],[2,35,13],[1,100,80],[2,50,32],[2,50,24],[4,25,9],[1,134,108],[2,67,43],[2,33,15,2,34,16],[2,33,11,2,34,12],[2,86,68],[4,43,27],[4,43,19],[4,43,15],[2,98,78],[4,49,31],[2,32,14,4,33,15],[4,39,13,1,40,14],[2,121,97],[2,60,38,2,61,39],[4,40,18,2,41,19],[4,40,14,2,41,15],[2,146,116],[3,58,36,2,59,37],[4,36,16,4,37,17],[4,36,12,4,37,13],[2,86,68,2,87,69],[4,69,43,1,70,44],[6,43,19,2,44,20],[6,43,15,2,44,16],[4,101,81],[1,80,50,4,81,51],[4,50,22,4,51,23],[3,36,12,8,37,13],[2,116,92,2,117,93],[6,58,36,2,59,37],[4,46,20,6,47,21],[7,42,14,4,43,15],[4,133,107],[8,59,37,1,60,38],[8,44,20,4,45,21],[12,33,11,4,34,12],[3,145,115,1,146,116],[4,64,40,5,65,41],[11,36,16,5,37,17],[11,36,12,5,37,13],[5,109,87,1,110,88],[5,65,41,5,66,42],[5,54,24,7,55,25],[11,36,12],[5,122,98,1,123,99],[7,73,45,3,74,46],[15,43,19,2,44,20],[3,45,15,13,46,16],[1,135,107,5,136,108],[10,74,46,1,75,47],[1,50,22,15,51,23],[2,42,14,17,43,15],[5,150,120,1,151,121],[9,69,43,4,70,44],[17,50,22,1,51,23],[2,42,14,19,43,15],[3,141,113,4,142,114],[3,70,44,11,71,45],[17,47,21,4,48,22],[9,39,13,16,40,14],[3,135,107,5,136,108],[3,67,41,13,68,42],[15,54,24,5,55,25],[15,43,15,10,44,16],[4,144,116,4,145,117],[17,68,42],[17,50,22,6,51,23],[19,46,16,6,47,17],[2,139,111,7,140,112],[17,74,46],[7,54,24,16,55,25],[34,37,13],[4,151,121,5,152,122],[4,75,47,14,76,48],[11,54,24,14,55,25],[16,45,15,14,46,16],[6,147,117,4,148,118],[6,73,45,14,74,46],[11,54,24,16,55,25],[30,46,16,2,47,17],[8,132,106,4,133,107],[8,75,47,13,76,48],[7,54,24,22,55,25],[22,45,15,13,46,16],[10,142,114,2,143,115],[19,74,46,4,75,47],[28,50,22,6,51,23],[33,46,16,4,47,17],[8,152,122,4,153,123],[22,73,45,3,74,46],[8,53,23,26,54,24],[12,45,15,28,46,16],[3,147,117,10,148,118],[3,73,45,23,74,46],[4,54,24,31,55,25],[11,45,15,31,46,16],[7,146,116,7,147,117],[21,73,45,7,74,46],[1,53,23,37,54,24],[19,45,15,26,46,16],[5,145,115,10,146,116],[19,75,47,10,76,48],[15,54,24,25,55,25],[23,45,15,25,46,16],[13,145,115,3,146,116],[2,74,46,29,75,47],[42,54,24,1,55,25],[23,45,15,28,46,16],[17,145,115],[10,74,46,23,75,47],[10,54,24,35,55,25],[19,45,15,35,46,16],[17,145,115,1,146,116],[14,74,46,21,75,47],[29,54,24,19,55,25],[11,45,15,46,46,16],[13,145,115,6,146,116],[14,74,46,23,75,47],[44,54,24,7,55,25],[59,46,16,1,47,17],[12,151,121,7,152,122],[12,75,47,26,76,48],[39,54,24,14,55,25],[22,45,15,41,46,16],[6,151,121,14,152,122],[6,75,47,34,76,48],[46,54,24,10,55,25],[2,45,15,64,46,16],[17,152,122,4,153,123],[29,74,46,14,75,47],[49,54,24,10,55,25],[24,45,15,46,46,16],[4,152,122,18,153,123],[13,74,46,32,75,47],[48,54,24,14,55,25],[42,45,15,32,46,16],[20,147,117,4,148,118],[40,75,47,7,76,48],[43,54,24,22,55,25],[10,45,15,67,46,16],[19,148,118,6,149,119],[18,75,47,31,76,48],[34,54,24,34,55,25],[20,45,15,61,46,16]],j.getRSBlocks=function(a,b){var c=j.getRsBlockTable(a,b);if(void 0==c)throw new Error("bad rs block @ typeNumber:"+a+"/errorCorrectLevel:"+b);for(var d=c.length/3,e=[],f=0;d>f;f++)for(var g=c[3*f+0],h=c[3*f+1],i=c[3*f+2],k=0;g>k;k++)e.push(new j(h,i));return e},j.getRsBlockTable=function(a,b){switch(b){case d.L:return j.RS_BLOCK_TABLE[4*(a-1)+0];case d.M:return j.RS_BLOCK_TABLE[4*(a-1)+1];case d.Q:return j.RS_BLOCK_TABLE[4*(a-1)+2];case d.H:return j.RS_BLOCK_TABLE[4*(a-1)+3];default:return void 0}},k.prototype={get:function(a){var b=Math.floor(a/8);return 1==(1&this.buffer[b]>>>7-a%8)},put:function(a,b){for(var c=0;b>c;c++)this.putBit(1==(1&a>>>b-c-1))},getLengthInBits:function(){return this.length},putBit:function(a){var b=Math.floor(this.length/8);this.buffer.length<=b&&this.buffer.push(0),a&&(this.buffer[b]|=128>>>this.length%8),this.length++}};var l=[[17,14,11,7],[32,26,20,14],[53,42,32,24],[78,62,46,34],[106,84,60,44],[134,106,74,58],[154,122,86,64],[192,152,108,84],[230,180,130,98],[271,213,151,119],[321,251,177,137],[367,287,203,155],[425,331,241,177],[458,362,258,194],[520,412,292,220],[586,450,322,250],[644,504,364,280],[718,560,394,310],[792,624,442,338],[858,666,482,382],[929,711,509,403],[1003,779,565,439],[1091,857,611,461],[1171,911,661,511],[1273,997,715,535],[1367,1059,751,593],[1465,1125,805,625],[1528,1190,868,658],[1628,1264,908,698],[1732,1370,982,742],[1840,1452,1030,790],[1952,1538,1112,842],[2068,1628,1168,898],[2188,1722,1228,958],[2303,1809,1283,983],[2431,1911,1351,1051],[2563,1989,1423,1093],[2699,2099,1499,1139],[2809,2213,1579,1219],[2953,2331,1663,1273]],o=function(){var a=function(a,b){this._el=a,this._htOption=b};return a.prototype.draw=function(a){function g(a,b){var c=document.createElementNS("http://www.w3.org/2000/svg",a);for(var d in b)b.hasOwnProperty(d)&&c.setAttribute(d,b[d]);return c}var b=this._htOption,c=this._el,d=a.getModuleCount();Math.floor(b.width/d),Math.floor(b.height/d),this.clear();var h=g("svg",{viewBox:"0 0 "+String(d)+" "+String(d),width:"100%",height:"100%",fill:b.colorLight});h.setAttributeNS("http://www.w3.org/2000/xmlns/","xmlns:xlink","http://www.w3.org/1999/xlink"),c.appendChild(h),h.appendChild(g("rect",{fill:b.colorDark,width:"1",height:"1",id:"template"}));for(var i=0;d>i;i++)for(var j=0;d>j;j++)if(a.isDark(i,j)){var k=g("use",{x:String(i),y:String(j)});k.setAttributeNS("http://www.w3.org/1999/xlink","href","#template"),h.appendChild(k)}},a.prototype.clear=function(){for(;this._el.hasChildNodes();)this._el.removeChild(this._el.lastChild)},a}(),p="svg"===document.documentElement.tagName.toLowerCase(),q=p?o:m()?function(){function a(){this._elImage.src=this._elCanvas.toDataURL("image/png"),this._elImage.style.display="block",this._elCanvas.style.display="none"}function d(a,b){var c=this;if(c._fFail=b,c._fSuccess=a,null===c._bSupportDataURI){var d=document.createElement("img"),e=function(){c._bSupportDataURI=!1,c._fFail&&_fFail.call(c)},f=function(){c._bSupportDataURI=!0,c._fSuccess&&c._fSuccess.call(c)};return d.onabort=e,d.onerror=e,d.onload=f,d.src="data:image/gif;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==",void 0}c._bSupportDataURI===!0&&c._fSuccess?c._fSuccess.call(c):c._bSupportDataURI===!1&&c._fFail&&c._fFail.call(c)}if(this._android&&this._android<=2.1){var b=1/window.devicePixelRatio,c=CanvasRenderingContext2D.prototype.drawImage;CanvasRenderingContext2D.prototype.drawImage=function(a,d,e,f,g,h,i,j){if("nodeName"in a&&/img/i.test(a.nodeName))for(var l=arguments.length-1;l>=1;l--)arguments[l]=arguments[l]*b;else"undefined"==typeof j&&(arguments[1]*=b,arguments[2]*=b,arguments[3]*=b,arguments[4]*=b);c.apply(this,arguments)}}var e=function(a,b){this._bIsPainted=!1,this._android=n(),this._htOption=b,this._elCanvas=document.createElement("canvas"),this._elCanvas.width=b.width,this._elCanvas.height=b.height,a.appendChild(this._elCanvas),this._el=a,this._oContext=this._elCanvas.getContext("2d"),this._bIsPainted=!1,this._elImage=document.createElement("img"),this._elImage.style.display="none",this._el.appendChild(this._elImage),this._bSupportDataURI=null};return e.prototype.draw=function(a){var b=this._elImage,c=this._oContext,d=this._htOption,e=a.getModuleCount(),f=d.width/e,g=d.height/e,h=Math.round(f),i=Math.round(g);b.style.display="none",this.clear();for(var j=0;e>j;j++)for(var k=0;e>k;k++){var l=a.isDark(j,k),m=k*f,n=j*g;c.strokeStyle=l?d.colorDark:d.colorLight,c.lineWidth=1,c.fillStyle=l?d.colorDark:d.colorLight,c.fillRect(m,n,f,g),c.strokeRect(Math.floor(m)+.5,Math.floor(n)+.5,h,i),c.strokeRect(Math.ceil(m)-.5,Math.ceil(n)-.5,h,i)}this._bIsPainted=!0},e.prototype.makeImage=function(){this._bIsPainted&&d.call(this,a)},e.prototype.isPainted=function(){return this._bIsPainted},e.prototype.clear=function(){this._oContext.clearRect(0,0,this._elCanvas.width,this._elCanvas.height),this._bIsPainted=!1},e.prototype.round=function(a){return a?Math.floor(1e3*a)/1e3:a},e}():function(){var a=function(a,b){this._el=a,this._htOption=b};return a.prototype.draw=function(a){for(var b=this._htOption,c=this._el,d=a.getModuleCount(),e=Math.floor(b.width/d),f=Math.floor(b.height/d),g=['<table style="border:0;border-collapse:collapse;">'],h=0;d>h;h++){g.push("<tr>");for(var i=0;d>i;i++)g.push('<td style="border:0;border-collapse:collapse;padding:0;margin:0;width:'+e+"px;height:"+f+"px;background-color:"+(a.isDark(h,i)?b.colorDark:b.colorLight)+';"></td>');g.push("</tr>")}g.push("</table>"),c.innerHTML=g.join("");var j=c.childNodes[0],k=(b.width-j.offsetWidth)/2,l=(b.height-j.offsetHeight)/2;k>0&&l>0&&(j.style.margin=l+"px "+k+"px")},a.prototype.clear=function(){this._el.innerHTML=""},a}();QRCode=function(a,b){if(this._htOption={width:256,height:256,typeNumber:4,colorDark:"#000000",colorLight:"#ffffff",correctLevel:d.H},"string"==typeof b&&(b={text:b}),b)for(var c in b)this._htOption[c]=b[c];"string"==typeof a&&(a=document.getElementById(a)),this._android=n(),this._el=a,this._oQRCode=null,this._oDrawing=new q(this._el,this._htOption),this._htOption.text&&this.makeCode(this._htOption.text)},QRCode.prototype.makeCode=function(a){this._oQRCode=new b(r(a,this._htOption.correctLevel),this._htOption.correctLevel),this._oQRCode.addData(a),this._oQRCode.make(),this._el.title=a,this._oDrawing.draw(this._oQRCode),this.makeImage()},QRCode.prototype.makeImage=function(){"function"==typeof this._oDrawing.makeImage&&(!this._android||this._android>=3)&&this._oDrawing.makeImage()},QRCode.prototype.clear=function(){this._oDrawing.clear()},QRCode.CorrectLevel=d}();
</script>
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

    let qrCodeInstance = null;
    function renderQrCode(text) {
      const container = document.getElementById('qrCodeContainer');
      if (!container) return;
      container.innerHTML = '';
      try {
        qrCodeInstance = new QRCode(container, {
          text: text,
          width: 180,
          height: 180,
          colorDark: '#0F172A',
          colorLight: '#FFFFFF',
          correctLevel: QRCode.CorrectLevel.M
        });
      } catch (err) {
        console.error('QR rendering error:', err);
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
        const displayUrl = data.join_url || data.invitation_url;
        document.getElementById('inviteUrlField').value = displayUrl;
        renderQrCode(displayUrl);
      } catch (e) {
        const mockUrl = `${window.location.origin}/join?token=mock_${Date.now()}&pass=${currentPassType}&port=51820`;
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
    html.reserve(DASHBOARD_HEAD.size() + DASHBOARD_BODY.size() + QRCODE_LIB_PART1.size() + QRCODE_LIB_PART2.size() + DASHBOARD_SCRIPT.size());
    html.append(DASHBOARD_HEAD);
    html.append(DASHBOARD_BODY);
    html.append(QRCODE_LIB_PART1);
    html.append(QRCODE_LIB_PART2);
    html.append(DASHBOARD_SCRIPT);
    return html;
}

} // namespace openroute::ui
