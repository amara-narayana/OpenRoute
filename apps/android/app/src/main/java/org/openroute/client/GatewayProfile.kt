package org.openroute.client

import android.net.Uri

data class GatewayProfile(
    val gatewayId: String,
    val gatewayName: String,
    val endpointHost: String,
    val endpointPort: Int,
    val gatewayPublicKeyBase64: String,
    val clientAssignedIp: String = "10.77.0.2",
    val passType: String = "TEMPORARY_24H",
    val passDurationSec: Long = 86400,
    val token: String = ""
) {
    companion object {
        fun parseFromUri(uriString: String): GatewayProfile? {
            try {
                val uri = Uri.parse(uriString)
                if (uri.scheme != "openroute" || uri.host != "invite") {
                    return null
                }

                val token = uri.getQueryParameter("token") ?: ""
                val gwId = uri.getQueryParameter("gw") ?: "gw_default"
                val name = uri.getQueryParameter("name") ?: "OpenRoute Gateway"
                val pub = uri.getQueryParameter("pub") ?: return null
                val port = uri.getQueryParameter("port")?.toIntOrNull() ?: 51820
                val host = uri.getQueryParameter("host") ?: "192.168.0.239"
                val pass = uri.getQueryParameter("pass") ?: "TEMPORARY_24H"
                val dur = uri.getQueryParameter("dur")?.toLongOrNull() ?: 86400L

                return GatewayProfile(
                    gatewayId = gwId,
                    gatewayName = name,
                    endpointHost = host,
                    endpointPort = port,
                    gatewayPublicKeyBase64 = pub,
                    passType = pass,
                    passDurationSec = dur,
                    token = token
                )
            } catch (e: Exception) {
                return null
            }
        }
    }
}
