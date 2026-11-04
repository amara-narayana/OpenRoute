package org.openroute.client

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

enum class ConnectionState {
    DISCONNECTED,
    CONNECTING,
    WAITING_APPROVAL,
    CONNECTED,
    RECONNECTING,
    FAILED
}

data class TunnelMetrics(
    val rttMs: Double = 0.0,
    val rxBps: Long = 0,
    val txBps: Long = 0,
    val totalBytes: Long = 0,
    val isDirect: Boolean = true
)

object TunnelManager {
    private val _connectionState = MutableStateFlow(ConnectionState.DISCONNECTED)
    val connectionState: StateFlow<ConnectionState> = _connectionState.asStateFlow()

    private val _metrics = MutableStateFlow(TunnelMetrics())
    val metrics: StateFlow<TunnelMetrics> = _metrics.asStateFlow()

    private val _currentProfile = MutableStateFlow<GatewayProfile?>(null)
    val currentProfile: StateFlow<GatewayProfile?> = _currentProfile.asStateFlow()

    fun setProfile(profile: GatewayProfile) {
        _currentProfile.value = profile
    }

    fun setState(state: ConnectionState) {
        _connectionState.value = state
    }

    fun updateMetrics(metrics: TunnelMetrics) {
        _metrics.value = metrics
    }
}
