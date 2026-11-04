package org.openroute.client

import android.app.Activity
import android.content.Intent
import android.net.VpnService
import android.os.Bundle
import android.view.View
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import com.journeyapps.barcodescanner.ScanContract
import com.journeyapps.barcodescanner.ScanOptions
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch
import org.openroute.client.databinding.ActivityMainBinding

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    private val vpnPrepareLauncher = registerForActivityResult(
        ActivityResultContracts.StartActivityForResult()
    ) { result ->
        if (result.resultCode == Activity.RESULT_OK) {
            startVpnService()
        } else {
            Toast.makeText(this, "VPN Permission is required to route traffic", Toast.LENGTH_SHORT).show()
        }
    }

    private val qrScanLauncher = registerForActivityResult(ScanContract()) { result ->
        if (result.contents != null) {
            handleInvitationUrl(result.contents)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        setupUI()
        observeState()
        handleIntent(intent)
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        handleIntent(intent)
    }

    private fun handleIntent(intent: Intent?) {
        val data = intent?.dataString
        if (!data.isNullOrEmpty()) {
            handleInvitationUrl(data)
        }
    }

    private fun handleInvitationUrl(url: String) {
        val profile = GatewayProfile.parseFromUri(url)
        if (profile != null) {
            TunnelManager.setProfile(profile)
            binding.tvGatewayName.text = profile.gatewayName
            binding.tvEndpoint.text = "${profile.endpointHost}:${profile.endpointPort}"
            Toast.makeText(this, "Imported gateway: ${profile.gatewayName}", Toast.LENGTH_SHORT).show()
        } else {
            Toast.makeText(this, "Invalid OpenRoute invitation link", Toast.LENGTH_SHORT).show()
        }
    }

    private fun setupUI() {
        binding.btnConnect.setOnClickListener {
            val currentState = TunnelManager.connectionState.value
            if (currentState == ConnectionState.CONNECTED || currentState == ConnectionState.CONNECTING) {
                stopVpnService()
            } else {
                prepareAndConnect()
            }
        }

        binding.btnScanQr.setOnClickListener {
            val options = ScanOptions().apply {
                setDesiredBarcodeFormats(ScanOptions.QR_CODE)
                setPrompt("Scan OpenRoute QR Invitation")
                setCameraId(0)
                setBeepEnabled(false)
                setBarcodeImageEnabled(false)
                setCaptureActivity(QrScannerActivity::class.java)
            }
            qrScanLauncher.launch(options)
        }
    }

    private fun prepareAndConnect() {
        if (TunnelManager.currentProfile.value == null) {
            Toast.makeText(this, "Please scan or import an invitation first", Toast.LENGTH_SHORT).show()
            return
        }

        val vpnIntent = VpnService.prepare(this)
        if (vpnIntent != null) {
            vpnPrepareLauncher.launch(vpnIntent)
        } else {
            startVpnService()
        }
    }

    private fun startVpnService() {
        val intent = Intent(this, OpenRouteVpnService::class.java).apply {
            action = OpenRouteVpnService.ACTION_CONNECT
        }
        ContextCompat.startForegroundService(this, intent)
    }

    private fun stopVpnService() {
        val intent = Intent(this, OpenRouteVpnService::class.java).apply {
            action = OpenRouteVpnService.ACTION_DISCONNECT
        }
        startService(intent)
    }

    private fun observeState() {
        lifecycleScope.launch {
            TunnelManager.connectionState.collectLatest { state ->
                when (state) {
                    ConnectionState.DISCONNECTED -> {
                        binding.tvConnectionStatus.text = getString(R.string.status_disconnected)
                        binding.viewStatusDot.setBackgroundColor(ContextCompat.getColor(this@MainActivity, R.color.text_secondary))
                        binding.btnConnect.text = getString(R.string.btn_connect)
                    }
                    ConnectionState.CONNECTING -> {
                        binding.tvConnectionStatus.text = getString(R.string.status_connecting)
                        binding.viewStatusDot.setBackgroundColor(ContextCompat.getColor(this@MainActivity, R.color.status_warning))
                        binding.btnConnect.text = getString(R.string.btn_disconnect)
                    }
                    ConnectionState.WAITING_APPROVAL -> {
                        binding.tvConnectionStatus.text = getString(R.string.status_waiting_approval)
                        binding.viewStatusDot.setBackgroundColor(ContextCompat.getColor(this@MainActivity, R.color.status_warning))
                        binding.btnConnect.text = getString(R.string.btn_disconnect)
                    }
                    ConnectionState.CONNECTED -> {
                        binding.tvConnectionStatus.text = getString(R.string.status_connected)
                        binding.viewStatusDot.setBackgroundColor(ContextCompat.getColor(this@MainActivity, R.color.status_success))
                        binding.btnConnect.text = getString(R.string.btn_disconnect)
                    }
                    ConnectionState.RECONNECTING -> {
                        binding.tvConnectionStatus.text = getString(R.string.status_reconnecting)
                        binding.viewStatusDot.setBackgroundColor(ContextCompat.getColor(this@MainActivity, R.color.status_warning))
                    }
                    ConnectionState.FAILED -> {
                        binding.tvConnectionStatus.text = "Connection Failed"
                        binding.viewStatusDot.setBackgroundColor(ContextCompat.getColor(this@MainActivity, R.color.status_error))
                        binding.btnConnect.text = getString(R.string.btn_connect)
                    }
                }
            }
        }

        lifecycleScope.launch {
            TunnelManager.metrics.collectLatest { metrics ->
                binding.tvLatency.text = if (metrics.rttMs > 0) "${metrics.rttMs.toInt()} ms" else "-- ms"
                val rxKbps = metrics.rxBps / 1024
                val txKbps = metrics.txBps / 1024
                binding.tvSpeed.text = "$rxKbps KB/s / $txKbps KB/s"
                val mb = metrics.totalBytes / (1024 * 1024)
                binding.tvTotalUsed.text = "$mb MB"
                binding.tvPathType.text = if (metrics.isDirect) "Direct Path" else "Relayed"
            }
        }
    }
}
