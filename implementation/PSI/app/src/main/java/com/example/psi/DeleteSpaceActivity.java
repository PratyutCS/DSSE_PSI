package com.example.psi;

import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

import com.example.psi.network.NetworkUtils;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class DeleteSpaceActivity extends AppCompatActivity {

    static {
        System.loadLibrary("psi");
    }
    private native void clearNativeDB(String storagePath);

    private void deleteRecursively(java.io.File fileOrDirectory) {
        if (fileOrDirectory.isDirectory()) {
            java.io.File[] children = fileOrDirectory.listFiles();
            if (children != null) {
                for (java.io.File child : children) {
                    deleteRecursively(child);
                }
            }
        }
        fileOrDirectory.delete();
    }

    private LinearLayout llSpaceList;
    private final ExecutorService executor = Executors.newSingleThreadExecutor();
    private final Handler handler = new Handler(Looper.getMainLooper());

    private static final String PREFS_NAME = "psi_prefs";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_list_generic); // Use generic list layout

        llSpaceList = findViewById(R.id.llSpaceList);
        
        TextView toolbarTitle = findViewById(R.id.toolbarTitle);
        toolbarTitle.setText("DELETE SPACE");
        
        View btnBack = findViewById(R.id.btnBack);
        btnBack.setVisibility(View.VISIBLE);
        btnBack.setOnClickListener(v -> finish());

        refreshList();
    }

    private void refreshList() {
        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        String token = prefs.getString("auth_token", null);
        if (token != null) fetchSpaces(token);
    }

    private void fetchSpaces(String token) {
        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        String ip = prefs.getString("last_ip", BuildConfig.SERVER_IP);
        String url = "http://" + ip + ":3000/api/get-spaces";

        executor.execute(() -> {
            try {
                String response = NetworkUtils.performGetRequest(url, token);
                JSONObject jsonResponse = new JSONObject(response);
                JSONArray spaces = jsonResponse.optJSONArray("spaces");
                handler.post(() -> updateSpaceList(spaces));
            } catch (Exception e) {
                e.printStackTrace();
            }
        });
    }

    private void updateSpaceList(JSONArray spaces) {
        llSpaceList.removeAllViews();
        if (spaces == null || spaces.length() == 0) {
            TextView empty = new TextView(this);
            empty.setText("No spaces to delete.");
            empty.setTextColor(getResources().getColor(R.color.text_dim));
            empty.setGravity(Gravity.CENTER);
            empty.setPadding(0, 50, 0, 0);
            llSpaceList.addView(empty);
            return;
        }

        LayoutInflater inflater = LayoutInflater.from(this);
        try {
            for (int i = 0; i < spaces.length(); i++) {
                String name = spaces.getString(i);
                View card = inflater.inflate(R.layout.item_space_card, llSpaceList, false);
                TextView tvName = card.findViewById(R.id.tvSpaceName);
                tvName.setText(name);
                tvName.setTextColor(0xFFFF4444); // Reddish for delete

                card.setOnClickListener(v -> deleteSpace(name, v));
                llSpaceList.addView(card);
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private void deleteSpace(String name, View view) {
        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        String token = prefs.getString("auth_token", null);
        String ip = prefs.getString("last_ip", BuildConfig.SERVER_IP);
        String url = "http://" + ip + ":3000/api/delete";

        // Grey out and disable to prevent double clicks
        view.setEnabled(false);
        view.setAlpha(0.5f);

        JSONObject body = new JSONObject();
        try { body.put("dbName", name); } catch (Exception e) {}

        executor.execute(() -> {
            try {
                NetworkUtils.performDeleteRequest(url, body.toString(), token);
                handler.post(() -> {
                    // Local Cleanup: Wipe out keys, counters, RocksDB locally too
                    String storagePath = new java.io.File(getFilesDir(), name).getAbsolutePath();
                    clearNativeDB(storagePath);
                    deleteRecursively(new java.io.File(storagePath));

                    Toast.makeText(this, "Space Deleted", Toast.LENGTH_SHORT).show();
                    refreshList();
                });
            } catch (Exception e) {
                e.printStackTrace();
                handler.post(() -> {
                    // Restore state on failure
                    view.setEnabled(true);
                    view.setAlpha(1.0f);
                    Toast.makeText(this, "Failed to delete: " + e.getMessage(), Toast.LENGTH_LONG).show();
                });
            }
        });
    }
}
