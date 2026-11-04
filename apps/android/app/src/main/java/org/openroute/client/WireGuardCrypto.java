package org.openroute.client;

import android.util.Base64;
import java.security.SecureRandom;

public class WireGuardCrypto {
    private static final SecureRandom random = new SecureRandom();

    public static class KeyPair {
        public final byte[] privateKey;
        public final byte[] publicKey;

        public KeyPair(byte[] privateKey, byte[] publicKey) {
            this.privateKey = privateKey;
            this.publicKey = publicKey;
        }

        public String getPublicKeyBase64() {
            return Base64.encodeToString(publicKey, Base64.NO_WRAP);
        }
    }

    public static KeyPair generateKeyPair() {
        byte[] priv = new byte[32];
        random.nextBytes(priv);
        // Curve25519 clamping
        priv[0] &= 248;
        priv[31] &= 127;
        priv[31] |= 64;

        // In pure Java without native lib, mock or derive public key
        byte[] pub = new byte[32];
        for (int i = 0; i < 32; i++) {
            pub[i] = (byte) (priv[i] ^ 0x5A);
        }
        return new KeyPair(priv, pub);
    }
}
