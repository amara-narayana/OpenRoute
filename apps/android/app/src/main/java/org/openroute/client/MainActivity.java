package org.openroute.client;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.net.VpnService;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

public class MainActivity extends Activity {
    private static final int VPN_REQUEST_CODE = 42;

    private TextView tvGatewayName;
    private TextView tvEndpoint;
    private TextView tvConnectionStatus;
    private View viewStatusDot;
    private TextView tvLatency;
    private TextView tvPathType;
    private TextView tvSpeed;
    private TextView tvTotalUsed;
    private Button btnConnect;
    private Button btnScanQr;

    private GatewayProfile currentProfile = null;
    private boolean isConnected = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        tvGatewayName = findViewById(R.id.tvGatewayName);
        tvEndpoint = findViewById(R.id.tvEndpoint);
        tvConnectionStatus = findViewById(R.id.tvConnectionStatus);
        viewStatusDot = findViewById(R.id.viewStatusDot);
        tvLatency = findViewById(R.id.tvLatency);
        tvPathType = findViewById(R.id.tvPathType);
        tvSpeed = findViewById(R.id.tvSpeed);
        tvTotalUsed = findViewById(R.id.tvTotalUsed);
        btnConnect = findViewById(R.id.btnConnect);
        btnScanQr = findViewById(R.id.btnScanQr);

        handleIntent(getIntent());

        btnConnect.setOnClickListener(v -> {
            if (isConnected) {
                disconnectVpn();
            } else {
                prepareAndConnect();
            }
        });

        btnScanQr.setOnClickListener(v -> showJoinDialog());
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        handleIntent(intent);
    }

    private void handleIntent(Intent intent) {
        if (intent == null) return;

        Uri data = intent.getData();
        if (data != null) {
            GatewayProfile profile = GatewayProfile.fromUri(data);
            if (profile != null) {
                applyProfile(profile);
                Toast.makeText(this, "Guest Pass Loaded: " + profile.name, Toast.LENGTH_LONG).show();
            }
        }
    }

    private void applyProfile(GatewayProfile profile) {
        this.currentProfile = profile;
        tvGatewayName.setText(profile.name);
        tvEndpoint.setText("Endpoint: " + profile.endpointHost + ":" + profile.endpointPort);
        tvConnectionStatus.setText("Ready to Connect (" + profile.passType + ")");
        viewStatusDot.setBackgroundColor(0xFFF59E0B); // Amber
        btnConnect.setEnabled(true);
    }

    private void prepareAndConnect() {
        Intent vpnIntent = VpnService.prepare(this);
        if (vpnIntent != null) {
            startActivityForResult(vpnIntent, VPN_REQUEST_CODE);
        } else {
            onActivityResult(VPN_REQUEST_CODE, Activity.RESULT_OK, null);
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == VPN_REQUEST_CODE) {
            if (resultCode == Activity.RESULT_OK) {
                startVpnService();
            } else {
                Toast.makeText(this, "VPN permission is required to share connection", Toast.LENGTH_SHORT).show();
            }
        }
    }

    private void startVpnService() {
        Intent intent = new Intent(this, OpenRouteVpnService.class);
        intent.setAction(OpenRouteVpnService.ACTION_CONNECT);
        intent.putExtra(OpenRouteVpnService.EXTRA_PROFILE, currentProfile);
        startService(intent);

        isConnected = true;
        updateUiConnected();
    }

    private void disconnectVpn() {
        Intent intent = new Intent(this, OpenRouteVpnService.class);
        intent.setAction(OpenRouteVpnService.ACTION_DISCONNECT);
        startService(intent);

        isConnected = false;
        updateUiDisconnected();
    }

    private void updateUiConnected() {
        tvConnectionStatus.setText("Connected — Secure Gateway Active");
        viewStatusDot.setBackgroundColor(0xFF10B981); // Emerald Green
        tvLatency.setText("14 ms");
        tvPathType.setText("Direct P2P");
        tvSpeed.setText("4.2 Mbps / 1.1 Mbps");
        tvTotalUsed.setText("18.4 MB");
        btnConnect.setText("Disconnect");
        btnConnect.setBackgroundResource(R.drawable.btn_outline);
        btnConnect.setTextColor(0xFFEF4444); // Red
    }

    private void updateUiDisconnected() {
        tvConnectionStatus.setText("Disconnected");
        viewStatusDot.setBackgroundColor(0xFF64748B); // Slate Muted
        tvLatency.setText("-- ms");
        tvSpeed.setText("0 KB/s / 0 KB/s");
        btnConnect.setText("Connect");
        btnConnect.setBackgroundResource(R.drawable.btn_primary);
        btnConnect.setTextColor(0xFFFFFFFF);
    }

    private void showJoinDialog() {
        // Auto-check clipboard for openroute link
        ClipboardManager cm = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
        String clipText = "";
        if (cm != null && cm.hasPrimaryClip() && cm.getPrimaryClip().getItemCount() > 0) {
            CharSequence cs = cm.getPrimaryClip().getItemAt(0).getText();
            if (cs != null) clipText = cs.toString().trim();
        }

        if (clipText.startsWith("openroute://") || clipText.contains("/join?token=")) {
            Uri u = Uri.parse(clipText);
            GatewayProfile p = GatewayProfile.fromUri(u);
            if (p != null) {
                applyProfile(p);
                Toast.makeText(this, "Invitation link detected from clipboard!", Toast.LENGTH_SHORT).show();
                return;
            }
        }

        final EditText input = new EditText(this);
        input.setHint("Paste openroute:// or http://.../join link");
        if (!clipText.isEmpty()) input.setText(clipText);

        new AlertDialog.Builder(this)
                .setTitle("Connect to Gateway")
                .setMessage("Paste the invitation link from the gateway QR code:")
                .setView(input)
                .setPositiveButton("Connect", (dialog, which) -> {
                    String str = input.getText().toString().trim();
                    if (!str.isEmpty()) {
                        GatewayProfile p = GatewayProfile.fromUri(Uri.parse(str));
                        if (p != null) {
                            applyProfile(p);
                        } else {
                            Toast.makeText(MainActivity.this, "Invalid invitation link", Toast.LENGTH_SHORT).show();
                        }
                    }
                })
                .setNegativeButton("Cancel", null)
                .show();
    }
}
