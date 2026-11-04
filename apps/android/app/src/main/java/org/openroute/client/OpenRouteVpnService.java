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
            builder.addDnsServer("10.77.0.1");
            builder.setMtu(1420);
            builder.setSession("OpenRoute: " + (activeProfile != null ? activeProfile.name : "Gateway"));

            vpnInterface = builder.establish();
            if (vpnInterface == null) {
                Log.e(TAG, "Failed to establish VPN interface");
                stopSelf();
                return;
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
        try {
            FileInputStream in = new FileInputStream(vpnInterface.getFileDescriptor());
            FileOutputStream out = new FileOutputStream(vpnInterface.getFileDescriptor());
            byte[] packet = new byte[32768];

            DatagramSocket udp = new DatagramSocket();
            protect(udp);

            InetAddress targetIp = InetAddress.getByName(activeProfile != null ? activeProfile.endpointHost : "192.168.0.239");
            int targetPort = activeProfile != null ? activeProfile.endpointPort : 51820;

            while (isRunning && !Thread.currentThread().isInterrupted()) {
                int length = in.read(packet);
                if (length > 0) {
                    DatagramPacket udpPkt = new DatagramPacket(packet, length, targetIp, targetPort);
                    udp.send(udpPkt);
                }
            }
            udp.close();
        } catch (Exception e) {
            if (isRunning) Log.w(TAG, "Tunnel loop interrupted: " + e.getMessage());
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
