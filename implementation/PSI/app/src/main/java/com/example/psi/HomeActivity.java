package com.example.psi;

import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

public class HomeActivity extends AppCompatActivity {

    private static final String PREFS_NAME = "psi_prefs";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_home);

        // TextView tvWelcome = findViewById(R.id.tvWelcome);
        Button btnLogout = findViewById(R.id.btnLogout);
        LinearLayout btnCreateSpace = findViewById(R.id.btnCreateSpace);
        LinearLayout btnSearchSpace = findViewById(R.id.btnSearchSpace);
        LinearLayout btnUpdateSpace = findViewById(R.id.btnUpdateSpace);
        LinearLayout btnDeleteSpace = findViewById(R.id.btnDeleteSpace);

        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        String token = prefs.getString("auth_token", null);

        if (token == null) {
            logout();
            return;
        }

        btnLogout.setOnClickListener(v -> logout());

        btnCreateSpace.setOnClickListener(v -> {
            startActivity(new Intent(this, CreateSpaceActivity.class));
        });

        btnSearchSpace.setOnClickListener(v -> {
            startActivity(new Intent(this, SearchActivity.class));
        });

        btnUpdateSpace.setOnClickListener(v -> {
            startActivity(new Intent(this, UpdateActivity.class));
        });

        btnDeleteSpace.setOnClickListener(v -> {
            startActivity(new Intent(this, DeleteSpaceActivity.class));
        });
    }

    private void logout() {
        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        prefs.edit().remove("auth_token").apply();

        Intent intent = new Intent(this, LoginActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
        startActivity(intent);
        finish();
    }
}
