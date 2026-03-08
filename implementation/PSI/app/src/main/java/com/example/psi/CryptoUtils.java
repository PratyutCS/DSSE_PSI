package com.example.psi;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Base64;

import java.security.SecureRandom;
import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;

public class CryptoUtils {
    private static final String PREFS_NAME = "psi_crypto_prefs";
    private static final String MASTER_KEY_ALIAS = "master_key";

    // Derives a 128-bit AES key for a specific database space
    public static byte[] getSpaceKey(Context context, String spaceName) {
        byte[] masterKey = getMasterKey(context);
        try {
            Mac mac = Mac.getInstance("HmacSHA256");
            mac.init(new SecretKeySpec(masterKey, "HmacSHA256"));
            byte[] derived = mac.doFinal(spaceName.getBytes());
            // Truncate or use first 16 bytes for AES-128
            byte[] key = new byte[16];
            System.arraycopy(derived, 0, key, 0, 16);
            return key;
        } catch (Exception e) {
            throw new RuntimeException("Key derivation failed", e);
        }
    }

    private static synchronized byte[] getMasterKey(Context context) {
        SharedPreferences prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        String encoded = prefs.getString(MASTER_KEY_ALIAS, null);
        if (encoded == null) {
            byte[] newKey = new byte[32]; // 256-bit master key
            new SecureRandom().nextBytes(newKey);
            encoded = Base64.encodeToString(newKey, Base64.DEFAULT);
            prefs.edit().putString(MASTER_KEY_ALIAS, encoded).apply();
            return newKey;
        }
        return Base64.decode(encoded, Base64.DEFAULT);
    }
}
