package com.example.psi;

import android.widget.TextView;

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

public class CreateSpaceActivity extends AppCompatActivity {

    private EditText etDbName;
    private Button btnProceed;
    private ProgressBar progressBar;

    private final ExecutorService executor = Executors.newSingleThreadExecutor();
    private final Handler handler = new Handler(Looper.getMainLooper());

    private static final String PREFS_NAME = "psi_prefs";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_create_space);

        etDbName = findViewById(R.id.etDbName);
        btnProceed = findViewById(R.id.btnProceed);
        progressBar = findViewById(R.id.progressBar);

        TextView toolbarTitle = findViewById(R.id.toolbarTitle);
        toolbarTitle.setText("INITIALIZE SPACE");
        
        View btnBack = findViewById(R.id.btnBack);
        btnBack.setVisibility(View.VISIBLE);
        btnBack.setOnClickListener(v -> finish());
        
        btnProceed.setOnClickListener(v -> createSpace());
    }

    private void createSpace() {
        String dbName = etDbName.getText().toString().trim();
        if (dbName.isEmpty()) {
            Toast.makeText(this, "Please enter a name", Toast.LENGTH_SHORT).show();
            return;
        }

        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        String token = prefs.getString("auth_token", null);
        String ip = prefs.getString("last_ip", BuildConfig.SERVER_IP);

        if (token == null) {
            finish();
            return;
        }

        String url = "http://" + ip + ":3000/api/new";
        showLoading(true);

        JSONObject body = new JSONObject();
        try {
            body.put("dbName", dbName);
        } catch (Exception e) {
            e.printStackTrace();
        }

        executor.execute(() -> {
            try {
                String response = NetworkUtils.performPostRequest(url, body.toString(), token);
                handler.post(() -> {
                    Toast.makeText(this, "Space Created Successfully!", Toast.LENGTH_SHORT).show();
                    finish();
                });
            } catch (Exception e) {
                e.printStackTrace();
                handler.post(() -> {
                    showLoading(false);
                    Toast.makeText(this, "Failed: " + e.getMessage(), Toast.LENGTH_LONG).show();
                });
            }
        });
    }

    private void showLoading(boolean loading) {
        progressBar.setVisibility(loading ? View.VISIBLE : View.GONE);
        btnProceed.setEnabled(!loading);
        etDbName.setEnabled(!loading);
    }
}
