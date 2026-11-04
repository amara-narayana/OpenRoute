package org.openroute.client;

import android.net.Uri;
import java.io.Serializable;

public class GatewayProfile implements Serializable {
    public final String gatewayId;
    public final String name;
    public final String endpointHost;
    public final int endpointPort;
    public final String publicKeyBase64;
    public final String assignedIp;
    public final String passType;
    public final long durationSec;
    public final String token;

    public GatewayProfile(String gatewayId, String name, String endpointHost, int endpointPort,
                          String publicKeyBase64, String assignedIp, String passType,
                          long durationSec, String token) {
        this.gatewayId = gatewayId;
        this.name = name;
        this.endpointHost = endpointHost;
        this.endpointPort = endpointPort;
        this.publicKeyBase64 = publicKeyBase64;
        this.assignedIp = assignedIp;
        this.passType = passType;
        this.durationSec = durationSec;
        this.token = token;
    }

    public static GatewayProfile fromUri(Uri uri) {
        if (uri == null) return null;

        String token = uri.getQueryParameter("token");
        if (token == null || token.isEmpty()) return null;

        String gw = uri.getQueryParameter("gw");
        if (gw == null) gw = "gw_default";

        String name = uri.getQueryParameter("name");
        if (name == null) name = "OpenRoute Gateway";

        String pub = uri.getQueryParameter("pub");
        if (pub == null) pub = "";

        int port = 51820;
        String portStr = uri.getQueryParameter("port");
        if (portStr != null) {
            try { port = Integer.parseInt(portStr); } catch (Exception ignored) {}
        }

        String pass = uri.getQueryParameter("pass");
        if (pass == null) pass = "TEMPORARY_24H";

        long dur = 86400;
        String durStr = uri.getQueryParameter("dur");
        if (durStr != null) {
            try { dur = Long.parseLong(durStr); } catch (Exception ignored) {}
        }

        // Host is either query param or from URI host
        String host = uri.getQueryParameter("host");
        if (host == null || host.isEmpty()) {
            host = uri.getHost();
        }
        if (host == null || host.equals("invite") || host.equals("join")) {
            host = "192.168.0.239"; // Gateway LAN IP fallback
        }

        return new GatewayProfile(gw, name, host, port, pub, "10.77.0.2", pass, dur, token);
    }
}
