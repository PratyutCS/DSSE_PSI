package com.example.psi;

import android.content.SharedPreferences;
import android.net.Uri;
import android.util.Log;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.AutoCompleteTextView;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;
import android.os.Environment;
import android.provider.Settings;
import android.content.Intent;
import android.os.Build;

import androidx.appcompat.app.AppCompatActivity;

import com.example.psi.network.NetworkUtils;

import org.json.JSONArray;
import org.json.JSONObject;
import java.io.File;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class SearchActivity extends AppCompatActivity {

    static {
        System.loadLibrary("psi");
    }

    private native String[] getSearchToken(String storagePath, String keyword);
    private native int[] performPostProcessing(String storagePath, int indexRange, String res1_0, String res1_1, String res2_0, String res2_1);
    private native boolean decryptResultFile(String storagePath, String encryptedPath, String decryptedPath);

    private AutoCompleteTextView actvSpaceSelector;
    private android.widget.EditText etParam1, etParam2;
    private Button btnConnect;
    private final ExecutorService executor = Executors.newSingleThreadExecutor();
    private final Handler handler = new Handler(Looper.getMainLooper());
    
    private TextView tvResultsTitle, tvResults;
    private View svResults;

    private static final String PREFS_NAME = "psi_prefs";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_search);

        actvSpaceSelector = findViewById(R.id.actvSpaceSelector);
        etParam1 = findViewById(R.id.etParam1);
        etParam2 = findViewById(R.id.etParam2);
        btnConnect = findViewById(R.id.btnConnect);
        
        TextView toolbarTitle = findViewById(R.id.toolbarTitle);
        toolbarTitle.setText("PRIVATE SEARCH");

        tvResultsTitle = findViewById(R.id.tvResultsTitle);
        tvResults = findViewById(R.id.tvResults);
        svResults = findViewById(R.id.svResults);
        
        View btnBack = findViewById(R.id.btnBack);
        btnBack.setVisibility(View.VISIBLE);
        btnBack.setOnClickListener(v -> finish());

        btnConnect.setOnClickListener(v -> performSearch());

        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        String token = prefs.getString("auth_token", null);

        if (token != null) {
            fetchSpaces(token);
        } else {
            finish();
        }
    }

    private void performSearch() {
        String dbName = actvSpaceSelector.getText().toString();
        String p1 = etParam1.getText().toString();
        String p2 = etParam2.getText().toString();

        if (dbName.isEmpty() || dbName.equals("Select Space")) {
            Toast.makeText(this, "Please select a space", Toast.LENGTH_SHORT).show();
            return;
        }
        if (p1.isEmpty() || p2.isEmpty()) {
            Toast.makeText(this, "Please enter both search parameters", Toast.LENGTH_SHORT).show();
            return;
        }

        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        String token = prefs.getString("auth_token", null);
        String ip = prefs.getString("last_ip", BuildConfig.SERVER_IP);
        String baseUrl = "http://" + ip + ":3000/api/get-index_value";

        Toast.makeText(this, "Searching...", Toast.LENGTH_SHORT).show();

        // 0. Check for MANAGE_EXTERNAL_STORAGE permission on Android 11+
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (!Environment.isExternalStorageManager()) {
                Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
                intent.setData(Uri.parse("package:" + getPackageName()));
                startActivity(intent);
                Toast.makeText(this, "Please grant 'All Files Access' to save results to SD card", Toast.LENGTH_LONG).show();
                return;
            }
        }

        executor.execute(() -> {
            try {
                String storagePath = getDbStoragePath(dbName);
                
                // 0. Cleanup previous searches
                File publicDecryptedDir = new File(Environment.getExternalStorageDirectory(), "PSI_SearchResults");
                clearDirectories(new File(getFilesDir(), "downloads"), publicDecryptedDir);
                
                // 1. Get tokens for param 1
                Log.i("PSI_SEARCH", "Generating token for P1: " + p1);
                String[] tokens1 = getSearchToken(storagePath, p1);
                Log.i("PSI_SEARCH", "P1 Tokens: u=" + tokens1[0] + ", count=" + tokens1[2]);
                
                String url1 = baseUrl + "?dbName=" + dbName + "&keyword_token=" + Uri.encode(tokens1[0]) + "&state_token=" + Uri.encode(tokens1[1]) + "&count=" + tokens1[2];
                Log.i("PSI_SEARCH", "Requesting P1 from server: " + url1);
                String response1 = NetworkUtils.performGetRequest(url1, token);
                JSONArray res1 = new JSONObject(response1).getJSONArray("results");
                Log.i("PSI_SEARCH", "P1 Results received: " + res1.length());

                // 2. Get tokens for param 2
                Log.i("PSI_SEARCH", "Generating token for P2: " + p2);
                String[] tokens2 = getSearchToken(storagePath, p2);
                Log.i("PSI_SEARCH", "P2 Tokens: u=" + tokens2[0] + ", count=" + tokens2[2]);
                
                String url2 = baseUrl + "?dbName=" + dbName + "&keyword_token=" + Uri.encode(tokens2[0]) + "&state_token=" + Uri.encode(tokens2[1]) + "&count=" + tokens2[2];
                Log.i("PSI_SEARCH", "Requesting P2 from server: " + url2);
                String response2 = NetworkUtils.performGetRequest(url2, token);
                JSONArray res2 = new JSONObject(response2).getJSONArray("results");
                Log.i("PSI_SEARCH", "P2 Results received: " + res2.length());

                // 3. Post Process
                // resX is a list of results. In the benchmark queen.cpp, search_result1[1] and [0] were used.
                // Assuming res1 has at least 2 elements [equal_id, boundary_val]
                String r10 = res1.length() > 0 ? res1.getString(0) : "-1";
                String r11 = res1.length() > 1 ? res1.getString(1) : "-1";
                String r20 = res2.length() > 0 ? res2.getString(0) : "-1";
                String r21 = res2.length() > 1 ? res2.getString(1) : "-1";

                int[] ids = performPostProcessing(storagePath, 100000, r10, r11, r20, r21);

                // 4. Download and Decrypt matched files
                File internalBase = getFilesDir();
                File downloadDir = new File(internalBase, "downloads");
                File decryptedDir = new File(Environment.getExternalStorageDirectory(), "PSI_SearchResults");
                
                if (!downloadDir.exists()) downloadDir.mkdirs();
                if (!decryptedDir.exists()) decryptedDir.mkdirs();

                for (int id : ids) {
                    String fileId = "ID" + id;
                    String downloadUrl = "http://" + ip + ":3000/api/download-file?dbName=" + dbName + "&fileId=" + fileId;
                    
                    Log.i("PSI_SEARCH", "Downloading " + fileId);
                    // downloadFile now returns the actual filename from the server (e.g. ID0.pdf)
                    String downloadedFileName = NetworkUtils.downloadFile(downloadUrl, downloadDir.getAbsolutePath(), token);
                    
                    File encFile = new File(downloadDir, downloadedFileName);
                    File decFile = new File(decryptedDir, downloadedFileName.replace(fileId, fileId + "_decrypted"));

                    Log.i("PSI_SEARCH", "Decrypting " + downloadedFileName);
                    boolean success = decryptResultFile(storagePath, encFile.getAbsolutePath(), decFile.getAbsolutePath());
                    if (success) {
                        Log.i("PSI_SEARCH", "Successfully decrypted " + downloadedFileName + " to " + decFile.getAbsolutePath());
                    } else {
                        Log.e("PSI_SEARCH", "Failed to decrypt " + downloadedFileName);
                    }
                }

                handler.post(() -> {
                    if (ids.length == 0) {
                        Toast.makeText(this, "No matching records found.", Toast.LENGTH_LONG).show();
                        tvResultsTitle.setVisibility(View.GONE);
                        svResults.setVisibility(View.GONE);
                    } else {
                        Toast.makeText(this, "Found " + ids.length + " matching IDs! Files saved in: " + decryptedDir.getAbsolutePath(), Toast.LENGTH_LONG).show();
                        
                        StringBuilder sb = new StringBuilder();
                        sb.append("Files recovered to: ").append(decryptedDir.getAbsolutePath()).append("\n\n");
                        for (int i = 0; i < ids.length; i++) {
                            sb.append("ID").append(ids[i]);
                            if (i < ids.length - 1) {
                                sb.append(", ");
                            }
                            if ((i + 1) % 5 == 0) {
                                sb.append("\n");
                            }
                        }
                        
                        tvResults.setText(sb.toString());
                        tvResultsTitle.setVisibility(View.VISIBLE);
                        svResults.setVisibility(View.VISIBLE);
                        
                        Log.i("PSI_SEARCH", "Matched IDs: " + sb.toString());
                    }
                });

            } catch (Exception e) {
                e.printStackTrace();
                handler.post(() -> Toast.makeText(this, "Search failed: " + e.getMessage(), Toast.LENGTH_LONG).show());
            }
        });
    }

    private void clearDirectories(File... dirs) {
        for (File dir : dirs) {
            if (dir != null && dir.exists() && dir.isDirectory()) {
                File[] files = dir.listFiles();
                if (files != null) {
                    for (File f : files) f.delete();
                }
            }
        }
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
                handler.post(() -> Toast.makeText(this, "Error: " + e.getMessage(), Toast.LENGTH_SHORT).show());
            }
        });
    }

    private void updateSpaceList(JSONArray spaces) {
        if (spaces == null || spaces.length() == 0) {
            actvSpaceSelector.setText("No secure spaces found.", false);
            return;
        }

        List<String> spaceNames = new ArrayList<>();
        try {
            for (int i = 0; i < spaces.length(); i++) {
                spaceNames.add(spaces.getString(i));
            }
        } catch (Exception e) {
            e.printStackTrace();
        }

        ArrayAdapter<String> adapter = new ArrayAdapter<>(
                this,
                R.layout.item_dropdown_luxury,
                spaceNames
        );
        actvSpaceSelector.setAdapter(adapter);
        
        if (!spaceNames.isEmpty()) {
            com.google.android.material.textfield.TextInputLayout til = findViewById(R.id.tilSpaceSelector);
            til.setHint("Select from " + spaceNames.size() + " spaces");
        }
    }
    private String getDbStoragePath(String dbName) {
        File dbDir = new File(getFilesDir(), dbName);
        if (!dbDir.exists()) dbDir.mkdirs();
        return dbDir.getAbsolutePath();
    }
}
