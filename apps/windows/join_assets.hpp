#pragma once

#include <string>
#include <string_view>

namespace openroute::ui {

inline constexpr std::string_view JOIN_HTML_HEAD = R"rawhtml(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Connect to OpenRoute Gateway</title>
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
      --radius: 16px;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; }
    body { background-color: var(--canvas); color: var(--text); min-height: 100vh; display: flex; flex-direction: column; align-items: center; justify-content: center; padding: 20px; }
    .container { width: 100%; max-width: 440px; background: var(--card-bg); border-radius: var(--radius); border: 1px solid var(--border); box-shadow: 0 10px 25px -5px rgba(0,0,0,0.06), 0 8px 10px -6px rgba(0,0,0,0.04); padding: 32px 24px; text-align: center; }
    .logo-badge { width: 56px; height: 56px; border-radius: 14px; background: var(--primary); color: white; font-size: 26px; font-weight: 800; display: inline-flex; align-items: center; justify-content: center; margin-bottom: 16px; box-shadow: 0 4px 12px rgba(37,99,235,0.25); }
    h1 { font-size: 22px; font-weight: 800; color: var(--text); letter-spacing: -0.3px; margin-bottom: 6px; }
    p.subtitle { font-size: 14px; color: var(--text-muted); line-height: 1.5; margin-bottom: 24px; }
    .pass-card { background: var(--primary-light); border: 1px solid #BFDBFE; border-radius: 12px; padding: 16px; text-align: left; margin-bottom: 24px; }
    .pass-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px; }
    .pass-title { font-size: 15px; font-weight: 700; color: var(--primary-dark); }
    .pill { display: inline-block; padding: 3px 8px; border-radius: 9999px; font-size: 11px; font-weight: 700; background: var(--success); color: white; text-transform: uppercase; }
    .pass-details { font-size: 13px; color: #1E40AF; line-height: 1.6; }
    .download-status-banner { background: var(--success-light); border: 1px solid #A7F3D0; border-radius: 10px; padding: 14px; margin-bottom: 20px; font-size: 13px; color: #065F46; display: flex; align-items: center; gap: 10px; font-weight: 600; text-align: left; }
    .spinner { width: 20px; height: 20px; border: 3px solid #6EE7B7; border-top-color: var(--success); border-radius: 50%; animation: spin 0.8s linear infinite; flex-shrink: 0; }
    @keyframes spin { to { transform: rotate(360deg); } }
    .btn { width: 100%; padding: 14px; border-radius: 10px; font-size: 15px; font-weight: 700; cursor: pointer; border: none; display: flex; align-items: center; justify-content: center; gap: 8px; transition: all 0.15s ease; text-decoration: none; margin-bottom: 12px; }
    .btn-primary { background: var(--primary); color: white; box-shadow: 0 4px 12px rgba(37,99,235,0.25); }
    .btn-primary:hover { background: var(--primary-dark); }
    .btn-outline { background: white; border: 1px solid var(--border); color: var(--text); }
    .btn-outline:hover { background: var(--canvas); }
    .steps-list { text-align: left; margin: 20px 0; display: flex; flex-direction: column; gap: 10px; font-size: 13px; color: var(--text-muted); }
    .step-item { display: flex; gap: 10px; align-items: flex-start; }
    .step-num { width: 20px; height: 20px; border-radius: 50%; background: #E2E8F0; color: var(--text); font-weight: 700; font-size: 11px; display: flex; align-items: center; justify-content: center; flex-shrink: 0; margin-top: 1px; }
    .footer-note { font-size: 11px; color: var(--text-muted); margin-top: 16px; }
  </style>
</head>
<body>
  <div class="container">
    <div class="logo-badge">OR</div>
    <h1>OpenRoute Gateway</h1>
    <p class="subtitle">Controlled remote internet-sharing platform.<br>Your secure guest pass is ready.</p>

    <div class="pass-card">
      <div class="pass-header">
        <span id="passName" class="pass-title">24-Hour Internet Pass</span>
        <span class="pill">Ready</span>
      </div>
      <div class="pass-details">
        <div>Gateway: <strong id="gwName">OpenRoute Gateway</strong></div>
        <div>Endpoint: <strong id="gwHost">Connecting...</strong></div>
        <div>Protection: <strong>Private LAN Isolation Active</strong></div>
      </div>
    </div>

    <!-- Auto-Download Notification -->
    <div id="downloadBanner" class="download-status-banner">
      <div class="spinner"></div>
      <div>
        <span>Downloading OpenRoute Android App...</span>
        <div style="font-size:11px; font-weight:400; opacity:0.85;">Tap 'Open' or 'Install' once download completes.</div>
      </div>
    </div>

    <!-- Manual Download Button -->
    <a id="downloadBtn" href="/download/openroute.apk" class="btn btn-primary" download>
      <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"></path><polyline points="7 10 12 15 17 10"></polyline><line x1="12" y1="15" x2="12" y2="3"></line></svg>
      Download OpenRoute App (.apk)
    </a>

    <!-- Already Installed Button -->
    <a id="openAppBtn" href="#" class="btn btn-outline">
      <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M18 13v6a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2h6"></path><polyline points="15 3 21 3 21 9"></polyline><line x1="10" y1="14" x2="21" y2="3"></line></svg>
      Already Installed? Connect Pass
    </a>

    <div class="steps-list">
      <div class="step-item"><span class="step-num">1</span><span>Install the downloaded <strong>openroute.apk</strong> on your phone.</span></div>
      <div class="step-item"><span class="step-num">2</span><span>Open the app and tap <strong>Connect</strong> — your pass is pre-configured!</span></div>
      <div class="step-item"><span class="step-num">3</span><span>The gateway owner approves access and you're online.</span></div>
    </div>

    <div class="footer-note">End-to-End Encrypted via WireGuard (Noise_IKpsk2). RFC 1918 LAN Guard.</div>
  </div>

  <script>
    // Extract parameters from URL
    const urlParams = new URLSearchParams(window.location.search);
    let token = urlParams.get('token') || '';
    let gw = urlParams.get('gw') || 'gw_default';
    let name = urlParams.get('name') || 'OpenRoute Gateway';
    let pub = urlParams.get('pub') || '';
    let host = urlParams.get('host') || window.location.hostname;
    let port = urlParams.get('port') || '51820';
    let pass = urlParams.get('pass') || 'TEMPORARY_24H';
    let dur = urlParams.get('dur') || '86400';

    if (token) token = token.replace(/ /g, '+');
    if (pub) pub = pub.replace(/ /g, '+');

    if (name) document.getElementById('gwName').innerText = name;
    if (host) document.getElementById('gwHost').innerText = host + ':' + port;
    if (pass === 'TEMPORARY_2H') document.getElementById('passName').innerText = '2-Hour Quick Guest Pass';
    else if (pass === 'TEMPORARY_24H') document.getElementById('passName').innerText = '24-Hour Day Pass';
    else if (pass === 'TEMPORARY_7D') document.getElementById('passName').innerText = '7-Day Weekly Pass';

    // Construct the direct deep link for when the app is installed
    const deepLink = `openroute://invite?token=${encodeURIComponent(token)}&gw=${encodeURIComponent(gw)}&name=${encodeURIComponent(name)}&pub=${encodeURIComponent(pub)}&host=${encodeURIComponent(host)}&port=${encodeURIComponent(port)}&pass=${encodeURIComponent(pass)}&dur=${encodeURIComponent(dur)}`;
    document.getElementById('openAppBtn').href = deepLink;

    // Save token to localStorage so if app opens in webview, token is preserved
    try {
      localStorage.setItem('openroute_invite_url', deepLink);
      localStorage.setItem('openroute_token', token);
    } catch(e) {}

    // Automatically trigger APK download via hidden link and iframe
    function triggerDownload() {
      try {
        const link = document.createElement('a');
        link.href = '/download/openroute.apk';
        link.download = 'openroute.apk';
        document.body.appendChild(link);
        link.click();
        document.body.removeChild(link);
      } catch(e) {}

      try {
        const iframe = document.createElement('iframe');
        iframe.style.display = 'none';
        iframe.src = '/download/openroute.apk';
        document.body.appendChild(iframe);
      } catch(e) {}
    }
    setTimeout(triggerDownload, 300);
  </script>
</body>
</html>
)rawhtml";

inline std::string GetJoinHtml() {
    return std::string(JOIN_HTML_HEAD);
}

} // namespace openroute::ui
