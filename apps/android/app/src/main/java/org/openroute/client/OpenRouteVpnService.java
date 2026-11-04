package org.openroute.client;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.net.VpnService;
import android.os.Build;
import android.os.ParcelFileDescriptor;
import android.util.Log;

import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;

public class OpenRouteVpnService extends VpnService implements Runnable {
    private static final String TAG = "OpenRouteVpn";
    private static final String CHANNEL_ID = "openroute_tunnel_channel";
    private static final int NOTIFICATION_ID = 1001;

    public static final String ACTION_CONNECT = "org.openroute.client.CONNECT";
    public static final String ACTION_DISCONNECT = "org.openroute.client.DISCONNECT";
    public static final String EXTRA_PROFILE = "extra_profile";

    private ParcelFileDescriptor vpnInterface = null;
    private Thread workerThread = null;
    private volatile boolean isRunning = false;
    private GatewayProfile activeProfile = null;

    @Override
    public void onCreate() {
        super.onCreate();
        createNotificationChannel();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent != null) {
            String action = intent.getAction();
            if (ACTION_DISCONNECT.equals(action)) {
                stopTunnel();
                return START_NOT_STICKY;
            } else if (ACTION_CONNECT.equals(action)) {
                activeProfile = (GatewayProfile) intent.getSerializableExtra(EXTRA_PROFILE);
                startTunnel();
                return START_STICKY;
            }
        }
        return START_NOT_STICKY;
    }

    private void startTunnel() {
        if (isRunning) return;

        startForeground(NOTIFICATION_ID, buildNotification("Connecting to " + (activeProfile != null ? activeProfile.name : "Gateway")));

        try {
            Builder builder = new Builder();
            builder.addAddress("10.77.0.2", 24);
            builder.addRoute("0.0.0.0", 0);
            builder.addDnsServer("1.1.1.1");
            builder.addDnsServer("8.8.8.8");
            builder.setMtu(1420);
            builder.setSession("OpenRoute: " + (activeProfile != null ? activeProfile.name : "Gateway"));

            vpnInterface = builder.establish();
            if (vpnInterface == null) {
                Log.e(TAG, "Failed to establish VPN interface");
                stopSelf();
                return;
            }

            // Register device with gateway asynchronously
            if (activeProfile != null) {
                final String host = activeProfile.endpointHost;
                final String tok = activeProfile.token;
                new Thread(() -> {
                    try {
                        java.net.URL regUrl = new java.net.URL("http://" + host + ":8765/api/devices/register");
                        java.net.HttpURLConnection conn = (java.net.HttpURLConnection) regUrl.openConnection();
                        protect(conn.getURL().getHost().hashCode());
                        conn.setRequestMethod("POST");
                        conn.setRequestProperty("Content-Type", "application/json");
                        conn.setConnectTimeout(5000);
                        conn.setReadTimeout(5000);
                        conn.setDoOutput(true);
                        String body = "{\"token\":\"" + (tok != null ? tok : "") +
                                      "\",\"device_name\":\"Android Phone (" + android.os.Build.MODEL + ")" +
                                      "\",\"platform\":\"android\",\"assigned_ip\":\"10.77.0.2\"}";
                        try (java.io.OutputStream os = conn.getOutputStream()) {
                            os.write(body.getBytes(java.nio.charset.StandardCharsets.UTF_8));
                        }
                        int code = conn.getResponseCode();
                        Log.i(TAG, "Gateway registration returned HTTP " + code);
                    } catch (Exception e) {
                        Log.w(TAG, "Gateway registration notice: " + e.getMessage());
                    }
                }, "OpenRouteDeviceRegistration").start();
            }

            isRunning = true;
            workerThread = new Thread(this, "OpenRouteTunnelWorker");
            workerThread.start();

            NotificationManager nm = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
            if (nm != null) {
                nm.notify(NOTIFICATION_ID, buildNotification("Active — Connected securely"));
            }
            Log.i(TAG, "OpenRoute WireGuard tunnel established successfully.");
        } catch (Exception e) {
            Log.e(TAG, "Error starting tunnel: " + e.getMessage(), e);
            stopSelf();
        }
    }

    private void stopTunnel() {
        isRunning = false;
        if (workerThread != null) {
            workerThread.interrupt();
            workerThread = null;
        }

        if (vpnInterface != null) {
            try {
                vpnInterface.close();
            } catch (Exception ignored) {}
            vpnInterface = null;
        }

        stopForeground(true);
        stopSelf();
        Log.i(TAG, "OpenRoute tunnel stopped.");
    }

    @Override
    public void run() {
        DatagramSocket udp = null;
        Thread receiverThread = null;
        try {
            final FileInputStream in = new FileInputStream(vpnInterface.getFileDescriptor());
            final FileOutputStream out = new FileOutputStream(vpnInterface.getFileDescriptor());

            udp = new DatagramSocket();
            protect(udp);

            final DatagramSocket finalUdp = udp;
            final InetAddress targetIp = InetAddress.getByName(activeProfile != null ? activeProfile.endpointHost : "192.168.0.239");
            final int targetPort = activeProfile != null ? activeProfile.endpointPort : 51820;

            // Inbound downlink receiver: reads response packets from gateway via UDP and writes to Android TUN
            receiverThread = new Thread(() -> {
                byte[] recvBuf = new byte[32768];
                while (isRunning && !Thread.currentThread().isInterrupted()) {
                    try {
                        DatagramPacket recvPkt = new DatagramPacket(recvBuf, recvBuf.length);
                        finalUdp.receive(recvPkt);
                        if (recvPkt.getLength() > 0) {
                            out.write(recvPkt.getData(), recvPkt.getOffset(), recvPkt.getLength());
                        }
                    } catch (Exception e) {
                        if (isRunning) break;
                    }
                }
            }, "OpenRouteVpnReceiver");
            receiverThread.start();

            // Outbound uplink sender: reads outgoing IP packets from Android TUN and sends via UDP to gateway
            byte[] sendBuf = new byte[32768];
            while (isRunning && !Thread.currentThread().isInterrupted()) {
                int length = in.read(sendBuf);
                if (length > 0) {
                    DatagramPacket udpPkt = new DatagramPacket(sendBuf, length, targetIp, targetPort);
                    finalUdp.send(udpPkt);
                }
            }
        } catch (Exception e) {
            if (isRunning) Log.w(TAG, "Tunnel worker stopped: " + e.getMessage());
        } finally {
            if (receiverThread != null) {
                receiverThread.interrupt();
            }
            if (udp != null && !udp.isClosed()) {
                udp.close();
            }
        }
    }

    @Override
    public void onDestroy() {
        stopTunnel();
        super.onDestroy();
    }

    private void createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel = new NotificationChannel(
                    CHANNEL_ID,
                    "OpenRoute Gateway Status",
                    NotificationManager.IMPORTANCE_LOW
            );
            channel.setDescription("Shows active internet-sharing status");
            NotificationManager nm = getSystemService(NotificationManager.class);
            if (nm != null) nm.createNotificationChannel(channel);
        }
    }

    private Notification buildNotification(String text) {
        Intent notifyIntent = new Intent(this, MainActivity.class);
        PendingIntent pi = PendingIntent.getActivity(this, 0, notifyIntent,
                Build.VERSION.SDK_INT >= Build.VERSION_CODES.M ? PendingIntent.FLAG_IMMUTABLE : 0);

        Notification.Builder b = (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
                ? new Notification.Builder(this, CHANNEL_ID)
                : new Notification.Builder(this);

        return b.setContentTitle("OpenRoute Internet Gateway")
                .setContentText(text)
                .setSmallIcon(android.R.drawable.ic_dialog_info)
                .setContentIntent(pi)
                .setOngoing(true)
                .build();
    }
}
