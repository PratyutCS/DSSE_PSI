package com.example.psi;

import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;

import androidx.appcompat.app.AppCompatActivity;

public class MainActivity extends AppCompatActivity {

    static {
        System.loadLibrary("psi");
    }

    public native void initializeNative(String storagePath);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        android.util.Log.d("PSI_APP", "MainActivity started");
        initializeNative(getFilesDir().getAbsolutePath());
        
        // Check for existing session
        SharedPreferences prefs = getSharedPreferences("psi_prefs", MODE_PRIVATE);
        String token = prefs.getString("auth_token", null);

        if (token != null) {
            // User is logged in, go to Home
            Intent intent = new Intent(this, HomeActivity.class);
            startActivity(intent);
        } else {
            // User not logged in, go to Login
            Intent intent = new Intent(this, LoginActivity.class);
            startActivity(intent);
        }
        
        // Finish MainActivity so user can't come back to it with Back button
        finish();
    }
}