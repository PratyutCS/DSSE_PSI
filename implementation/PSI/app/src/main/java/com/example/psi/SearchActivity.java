package com.example.psi;

import android.content.SharedPreferences;
import android.net.Uri;
import android.util.Log;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.AutoCompleteTextView;
import android.widget.Button;
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

    static { System.loadLibrary("psi"); }

    // JNI: returns 12 strings [stk1_l(kw,st,c), stk1_e(kw,st,c), stk2_l(kw,st,c), stk2_e(kw,st,c)]
    private native String[] getSearchTokens(String storagePath, byte[] key, int a, int b, int kwSpaceSize);
    // JNI: bitmap post-processing
    private native int[] performPostProcessing(int numIDs, int kwSpaceSize,
                                                String[] r1l, String[] r1e, String[] r2l, String[] r2e,
                                                int a, int b);
    // JNI: decrypt a downloaded file
    private native boolean decryptResultFile(String storagePath, byte[] key, String encryptedPath, String decryptedPath);

    private AutoCompleteTextView actvSpaceSelector;
    private android.widget.EditText etParam1, etParam2;
    private Button btnConnect;
    private final ExecutorService executor = Executors.newSingleThreadExecutor();
    private final Handler handler = new Handler(Looper.getMainLooper());

    private TextView tvResultsTitle, tvResults;
    private View svResults;

    private static final String TAG = "PSI_SEARCH";
    private static final String PREFS_NAME = "psi_prefs";
    private static final int KW_SPACE_SIZE = 100000;

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
        tvResults      = findViewById(R.id.tvResults);
        svResults      = findViewById(R.id.svResults);

        View btnBack = findViewById(R.id.btnBack);
        btnBack.setVisibility(View.VISIBLE);
        btnBack.setOnClickListener(v -> finish());

        btnConnect.setOnClickListener(v -> performSearch());

        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        String token = prefs.getString("auth_token", null);
        if (token != null) fetchSpaces(token);
        else finish();
    }

    private void performSearch() {
        String dbName = actvSpaceSelector.getText().toString().trim();
        String p1 = etParam1.getText().toString().trim();
        String p2 = etParam2.getText().toString().trim();

        if (dbName.isEmpty() || dbName.equals("Select Space")) {
            Toast.makeText(this, "Please select a space", Toast.LENGTH_SHORT).show();
            return;
        }
        if (p1.isEmpty() || p2.isEmpty()) {
            Toast.makeText(this, "Please enter both range parameters", Toast.LENGTH_SHORT).show();
            return;
        }

        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        String token  = prefs.getString("auth_token", null);
        String ip     = prefs.getString("last_ip", BuildConfig.SERVER_IP);
        String searchUrl = "http://" + ip + ":3000/api/get-index_value";

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && !Environment.isExternalStorageManager()) {
            startActivity(new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                    Uri.parse("package:" + getPackageName())));
            Toast.makeText(this, "Grant 'All Files Access' first", Toast.LENGTH_LONG).show();
            return;
        }

        Toast.makeText(this, "Searching…", Toast.LENGTH_SHORT).show();

        executor.execute(() -> {
            try {
                File downloadDir  = new File(getFilesDir(), "downloads");
                File decryptedDir = new File(Environment.getExternalStorageDirectory(), "PSI_SearchResults");
                clearDirectories(downloadDir, decryptedDir);

                String storagePath = getDbStoragePath(dbName);
                byte[] spaceKey = CryptoUtils.getSpaceKey(this, dbName);
                int a = Integer.parseInt(p1);
                int b = Integer.parseInt(p2);

                Log.i(TAG, "Generating search tokens for [" + a + ", " + b + "]");
                String[] tokens = getSearchTokens(storagePath, spaceKey, a, b, KW_SPACE_SIZE);
                if (tokens == null || tokens.length < 12)
                    throw new Exception("getSearchTokens returned invalid data");

                String[][] serverResults = new String[4][];
                for (int i = 0; i < 4; i++) {
                    int off = i * 3;
                    String countStr = tokens[off + 2];
                    if ("0".equals(countStr)) {
                        serverResults[i] = new String[0];
                        continue;
                    }
                    String url = searchUrl
                            + "?dbName="        + Uri.encode(dbName)
                            + "&keyword_token=" + Uri.encode(tokens[off])
                            + "&state_token="   + Uri.encode(tokens[off + 1])
                            + "&count="         + countStr;
                    try {
                        String resp = NetworkUtils.performGetRequest(url, token);
                        JSONArray arr = new JSONObject(resp).getJSONArray("results");
                        serverResults[i] = new String[arr.length()];
                        for (int j = 0; j < arr.length(); j++)
                            serverResults[i][j] = arr.getString(j);
                    } catch (Exception e) {
                        serverResults[i] = new String[0];
                    }
                }

                // Fetch current fileCount for bitmap dimensioning
                String infoUrl = "http://" + ip + ":3000/api/get-space-info?dbName=" + Uri.encode(dbName);
                String infoResp = NetworkUtils.performGetRequest(infoUrl, token);
                int numIDs = new JSONObject(infoResp).getInt("fileCount");

                int[] matchedIds = performPostProcessing(numIDs, KW_SPACE_SIZE,
                        serverResults[0], serverResults[1],
                        serverResults[2], serverResults[3],
                        a, b);

                if (!downloadDir.exists()) downloadDir.mkdirs();
                if (!decryptedDir.exists()) decryptedDir.mkdirs();

                for (int id : matchedIds) {
                    String fileId = "ID" + id;
                    String dlUrl = "http://" + ip + ":3000/api/download-file?dbName=" + dbName + "&fileId=" + fileId;
                    try {
                        String dlName = NetworkUtils.downloadFile(dlUrl, downloadDir.getAbsolutePath(), token);
                        File encFile = new File(downloadDir, dlName);
                        File decFile = new File(decryptedDir, dlName.replace(fileId, fileId + "_decrypted"));

                        decryptResultFile(storagePath, spaceKey, encFile.getAbsolutePath(), decFile.getAbsolutePath());
                    } catch (Exception e) {
                        Log.e(TAG, "Download/decrypt error for " + fileId, e);
                    }
                }

                handler.post(() -> {
                    if (matchedIds.length == 0) {
                        Toast.makeText(this, "No matching records", Toast.LENGTH_LONG).show();
                        tvResultsTitle.setVisibility(View.GONE);
                        svResults.setVisibility(View.GONE);
                    } else {
                        StringBuilder sb = new StringBuilder();
                        sb.append("Files recovered to: ").append(decryptedDir.getAbsolutePath()).append("\n\n");
                        for (int i = 0; i < matchedIds.length; i++) {
                            sb.append("ID").append(matchedIds[i]);
                            if (i < matchedIds.length - 1) sb.append(", ");
                            if ((i + 1) % 5 == 0) sb.append("\n");
                        }
                        tvResults.setText(sb.toString());
                        tvResultsTitle.setVisibility(View.VISIBLE);
                        svResults.setVisibility(View.VISIBLE);
                        Toast.makeText(this, "Found " + matchedIds.length + " match(es)!", Toast.LENGTH_LONG).show();
                    }
                });

            } catch (Exception e) {
                Log.e(TAG, "Search failed", e);
                handler.post(() -> Toast.makeText(this, "Search failed: " + e.getMessage(), Toast.LENGTH_LONG).show());
            }
        });
    }

    private void clearDirectories(File... dirs) {
        for (File dir : dirs) {
            if (dir != null && dir.exists() && dir.isDirectory()) {
                File[] files = dir.listFiles();
                if (files != null) for (File f : files) f.delete();
            }
        }
    }

    private void fetchSpaces(String token) {
        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        String ip  = prefs.getString("last_ip", BuildConfig.SERVER_IP);
        String url = "http://" + ip + ":3000/api/get-spaces";

        executor.execute(() -> {
            try {
                String resp = NetworkUtils.performGetRequest(url, token);
                JSONArray spaces = new JSONObject(resp).optJSONArray("spaces");
                handler.post(() -> updateSpaceList(spaces));
            } catch (Exception e) {
                Log.e(TAG, "fetchSpaces error", e);
            }
        });
    }

    private void updateSpaceList(JSONArray spaces) {
        if (spaces == null || spaces.length() == 0) {
            actvSpaceSelector.setText("No spaces found.", false);
            return;
        }
        List<String> names = new ArrayList<>();
        try {
            for (int i = 0; i < spaces.length(); i++) names.add(spaces.getString(i));
        } catch (Exception ignored) {}

        actvSpaceSelector.setAdapter(new ArrayAdapter<>(this, R.layout.item_dropdown_luxury, names));

        if (!names.isEmpty()) {
            com.google.android.material.textfield.TextInputLayout til = findViewById(R.id.tilSpaceSelector);
            if (til != null) til.setHint("Select from " + names.size() + " spaces");
        }
    }

    private String getDbStoragePath(String dbName) {
        File dir = new File(getFilesDir(), dbName);
        if (!dir.exists()) dir.mkdirs();
        return dir.getAbsolutePath();
    }
}
