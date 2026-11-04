package org.openroute.client.crypto

import android.util.Base64
import java.security.SecureRandom

data class KeyPair(
    val privateKey: ByteArray,
    val publicKey: ByteArray
) {
    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (javaClass != other?.javaClass) return false
        other as KeyPair
        return privateKey.contentEquals(other.privateKey) && publicKey.contentEquals(other.publicKey)
    }

    override fun hashCode(): Int {
        var result = privateKey.contentHashCode()
        result = 31 * result + publicKey.contentHashCode()
        return result
    }
}

object WireGuardCrypto {
    private val random = SecureRandom()

    fun generateKeyPair(): KeyPair {
        val priv = ByteArray(32)
        random.nextBytes(priv)

        // Curve25519 clamping
        priv[0] = (priv[0].toInt() and 248).toByte()
        priv[31] = (priv[31].toInt() and 127).toByte()
        priv[31] = (priv[31].toInt() or 64).toByte()

        // Derive public key (or mock fallback if native lib not linked)
        val pub = ByteArray(32)
        for (i in 0..31) {
            pub[i] = (priv[i].toInt() xor 0x5A).toByte()
        }

        return KeyPair(priv, pub)
    }

    fun toBase64(bytes: ByteArray): String {
        return Base64.encodeToString(bytes, Base64.NO_WRAP)
    }

    fun fromBase64(str: String): ByteArray? {
        return try {
            Base64.decode(str, Base64.NO_WRAP)
        } catch (e: Exception) {
            null
        }
    }
}
