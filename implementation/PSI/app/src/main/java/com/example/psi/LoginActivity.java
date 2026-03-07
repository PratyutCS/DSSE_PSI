package com.example.psi;

import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ProgressBar;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

import com.example.psi.network.NetworkUtils;

import org.json.JSONObject;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class LoginActivity extends AppCompatActivity {

    private EditText etServerIp;
    private EditText etUsername;
    private EditText etPassword;
    private Button btnLogin;
    private ProgressBar progressBar;

    private final ExecutorService executor = Executors.newSingleThreadExecutor();
    private final Handler handler = new Handler(Looper.getMainLooper());

    private static final String PREFS_NAME = "psi_prefs";
    private static final String PREF_LAST_IP = "last_ip";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_login);
        android.util.Log.d("PSI_APP", "LoginActivity started");

        etServerIp = findViewById(R.id.etServerIp);
        etUsername = findViewById(R.id.etUsername);
        etPassword = findViewById(R.id.etPassword);
        btnLogin = findViewById(R.id.btnLogin);
        progressBar = findViewById(R.id.progressBar);

        // Load last used IP
        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        String lastIp = prefs.getString(PREF_LAST_IP, BuildConfig.SERVER_IP);
        etServerIp.setText(lastIp);

        btnLogin.setOnClickListener(v -> attemptLogin());
    }

    private void attemptLogin() {
        String ip = etServerIp.getText().toString().trim();
        String username = etUsername.getText().toString();
        String password = etPassword.getText().toString();

        if (ip.isEmpty() || username.isEmpty() || password.isEmpty()) {
            Toast.makeText(this, "Please fill all fields", Toast.LENGTH_SHORT).show();
            return;
        }

        // Save IP for convenience
        getSharedPreferences(PREFS_NAME, MODE_PRIVATE)
                .edit()
                .putString(PREF_LAST_IP, ip)
                .apply();

        String loginUrl = "http://" + ip + ":3000/api/login";

        showLoading(true);

        JSONObject jsonBody = new JSONObject();
        try {
            jsonBody.put("username", username);
            jsonBody.put("password", password);
        } catch (Exception e) {
            e.printStackTrace();
        }

        executor.execute(() -> {
            try {
                String response = NetworkUtils.performPostRequest(loginUrl, jsonBody.toString(), null);
                
                JSONObject jsonResponse = new JSONObject(response);
                String token = jsonResponse.optString("token");

                if (!token.isEmpty()) {
                    handler.post(() -> onLoginSuccess(token));
                } else {
                    throw new Exception("Token not found in response");
                }

            } catch (Exception e) {
                handler.post(() -> onLoginFailed(e.getMessage()));
            }
        });
    }

    private void onLoginSuccess(String token) {
        showLoading(false);
        Toast.makeText(this, "Login Successful!", Toast.LENGTH_SHORT).show();

        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        prefs.edit().putString("auth_token", token).apply();

        startActivity(new Intent(this, HomeActivity.class));
        finish();
    }

    private void onLoginFailed(String error) {
        showLoading(false);
        Toast.makeText(this, "Login Failed: " + error, Toast.LENGTH_LONG).show();
    }

    private void showLoading(boolean isLoading) {
        progressBar.setVisibility(isLoading ? View.VISIBLE : View.GONE);
        btnLogin.setEnabled(!isLoading);
        etServerIp.setEnabled(!isLoading);
        etUsername.setEnabled(!isLoading);
        etPassword.setEnabled(!isLoading);
    }
}
