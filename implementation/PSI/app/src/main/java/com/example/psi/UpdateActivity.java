package com.example.psi;

import android.content.SharedPreferences;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.provider.OpenableColumns;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.example.psi.network.NetworkUtils;
import org.json.JSONArray;
import org.json.JSONObject;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;

public class UpdateActivity extends AppCompatActivity {
    static { System.loadLibrary("psi"); }

    // JNI: Add files → returns [u0,e0,…, "---FILES---", path0,path1,…]
    private native String[] generateUpdateTokens(String storagePath, byte[] key, int kwSpaceSize, String[] filePaths, int[] keywords, int startingIdIndex);
    // JNI: Delete signal → returns [u0,e0,…]
    private native String[] generateDeleteTokens(String storagePath, byte[] key, int kwSpaceSize, String[] identifiers, int[] keywords);

    private static final String TAG = "PSI_UPDATE";
    private static final String PREFS_NAME = "psi_prefs";
    private static final int KW_SPACE_SIZE = 1000;

    // UI
    private Spinner spinnerSpaces;
    private Button btnModeAdd, btnModeDelete, btnPerformUpdate, btnAddFile;
    private LinearLayout layoutAddFile, layoutDeleteSignal;
    private EditText etTargetId, etTargetKeywords;
    private RecyclerView rvFiles;
    private TextView tvEmptyMessage, tvProgress;
    private android.widget.ProgressBar progressBar;

    private FileAdapter fileAdapter;
    private final List<FileItem> selectedFiles = new ArrayList<>();
    private boolean isAddMode = true;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_update);

        spinnerSpaces       = findViewById(R.id.spinnerSpaces);
        btnModeAdd          = findViewById(R.id.btnModeAdd);
        btnModeDelete       = findViewById(R.id.btnModeDelete);
        btnPerformUpdate    = findViewById(R.id.btnPerformUpdate);
        btnAddFile          = findViewById(R.id.btnAddFile);
        layoutAddFile       = findViewById(R.id.layoutAddFile);
        layoutDeleteSignal  = findViewById(R.id.layoutDeleteSignal);
        etTargetId          = findViewById(R.id.etTargetId);
        etTargetKeywords    = findViewById(R.id.etTargetKeywords);
        rvFiles             = findViewById(R.id.rvFiles);
        tvEmptyMessage      = findViewById(R.id.tvEmptyMessage);
        tvProgress          = findViewById(R.id.tvProgress);
        progressBar         = findViewById(R.id.progressBar);

        TextView toolbarTitle = findViewById(R.id.toolbarTitle);
        toolbarTitle.setText("UPDATE PSI");
        View btnBack = findViewById(R.id.btnBack);
        btnBack.setVisibility(View.VISIBLE);
        btnBack.setOnClickListener(v -> finish());

        rvFiles.setLayoutManager(new LinearLayoutManager(this));
        fileAdapter = new FileAdapter(selectedFiles, this::checkUpdateState);
        rvFiles.setAdapter(fileAdapter);

        btnAddFile.setOnClickListener(v -> filePickerLauncher.launch("*/*"));
        btnPerformUpdate.setOnClickListener(v -> performUpdate());

        btnModeAdd.setOnClickListener(v -> setMode(true));
        btnModeDelete.setOnClickListener(v -> setMode(false));

        spinnerSpaces.setOnItemSelectedListener(new android.widget.AdapterView.OnItemSelectedListener() {
            @Override public void onItemSelected(android.widget.AdapterView<?> p, View v, int pos, long id) { checkUpdateState(); }
            @Override public void onNothingSelected(android.widget.AdapterView<?> p) { checkUpdateState(); }
        });

        fetchAllSpaces();
    }

    private void setMode(boolean addMode) {
        isAddMode = addMode;
        layoutAddFile.setVisibility(addMode ? View.VISIBLE : View.GONE);
        layoutDeleteSignal.setVisibility(addMode ? View.GONE : View.VISIBLE);

        btnModeAdd.setBackgroundResource(addMode ? R.drawable.bg_button_gold : R.drawable.bg_button_outline);
        btnModeDelete.setBackgroundResource(addMode ? R.drawable.bg_button_outline : R.drawable.bg_button_gold);
        btnModeAdd.setTextColor(addMode ? 0xFFFFFFFF : getResources().getColor(R.color.gold));
        btnModeDelete.setTextColor(addMode ? getResources().getColor(R.color.gold) : 0xFFFFFFFF);

        btnPerformUpdate.setText(addMode ? "PERFORM UPDATE" : "SEND DELETE SIGNAL");
        checkUpdateState();
    }

    private final ActivityResultLauncher<String> filePickerLauncher = registerForActivityResult(
            new ActivityResultContracts.GetMultipleContents(), this::handleSelectedFiles);

    private void handleSelectedFiles(List<Uri> uris) {
        for (Uri uri : uris) {
            String path = copyUriToInternalStorage(uri);
            if (path != null)
                selectedFiles.add(new FileItem(getFileName(uri), "0", path)); // Default keyword = 0
        }
        fileAdapter.notifyDataSetChanged();
        checkUpdateState();
    }

    private void checkUpdateState() {
        String space = spinnerSpaces.getSelectedItem() != null ? spinnerSpaces.getSelectedItem().toString() : "";
        boolean validSpace = !space.isEmpty() && !space.equals("No spaces available");

        boolean canUpdate;
        if (isAddMode) {
            canUpdate = validSpace && !selectedFiles.isEmpty();
        } else {
            String id = etTargetId.getText().toString().trim();
            String kw = etTargetKeywords.getText().toString().trim();
            canUpdate = validSpace && !id.isEmpty() && !kw.isEmpty();
        }
        btnPerformUpdate.setEnabled(canUpdate);
        btnPerformUpdate.setAlpha(canUpdate ? 1.0f : 0.5f);
    }

    private void performUpdate() {
        String dbName = spinnerSpaces.getSelectedItem().toString();
        int maxKw = KW_SPACE_SIZE - 1;
        
        // Input validation before starting background work
        if (isAddMode) {
            for (FileItem item : selectedFiles) {
                try {
                    int val = item.keyword.isEmpty() ? 0 : Integer.parseInt(item.keyword);
                    if (val < 0 || val > maxKw) {
                        Toast.makeText(this, "File '" + item.name + "' has keyword out of range 0-" + maxKw, Toast.LENGTH_LONG).show();
                        return;
                    }
                } catch (NumberFormatException e) {
                    Toast.makeText(this, "File '" + item.name + "' has an invalid keyword number.", Toast.LENGTH_LONG).show();
                    return;
                }
            }
        } else {
            try {
                int kw = Integer.parseInt(etTargetKeywords.getText().toString().trim());
                if (kw < 0 || kw > maxKw) {
                    Toast.makeText(this, "Delete keyword must be 0 to " + maxKw, Toast.LENGTH_LONG).show();
                    etTargetKeywords.setError("Must be 0 to " + maxKw);
                    return;
                }
            } catch (NumberFormatException e) {
                Toast.makeText(this, "Invalid delete target keyword number.", Toast.LENGTH_LONG).show();
                return;
            }
        }

        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        String token = prefs.getString("auth_token", null);
        String ip    = prefs.getString("last_ip", BuildConfig.SERVER_IP);
        String baseUrl = "http://" + ip + ":3000/api";

        setUIEnabled(false);
        tvProgress.setVisibility(View.VISIBLE);
        progressBar.setVisibility(View.VISIBLE);
        progressBar.setProgress(0);
        tvProgress.setText("Initializing Cryptography...");

        new Thread(() -> {
            try {
                String storagePath = getDbStoragePath(dbName);
                byte[] spaceKey = CryptoUtils.getSpaceKey(this, dbName);
                String[] jniResult;

                if (isAddMode) {
                    // ---- ADD mode ----
                    runOnUiThread(() -> tvProgress.setText("Generating Secure Tokens (0%)"));
                    String[] paths = new String[selectedFiles.size()];
                    int[] kws      = new int[selectedFiles.size()];
                    for (int i = 0; i < selectedFiles.size(); i++) {
                        paths[i] = selectedFiles.get(i).localPath;
                        String kwStr = selectedFiles.get(i).keyword;
                        kws[i] = kwStr.isEmpty() ? 0 : Integer.parseInt(kwStr);
                    }

                    // Fetch server-side fileCount for unique IDs
                    String infoUrl = baseUrl + "/get-space-info?dbName=" + Uri.encode(dbName);
                    String infoResp = NetworkUtils.performGetRequest(infoUrl, token);
                    int startingId = new JSONObject(infoResp).getInt("fileCount");

                    jniResult = generateUpdateTokens(storagePath, spaceKey, KW_SPACE_SIZE, paths, kws, startingId);
                    if (jniResult == null) throw new Exception("Native token generation returned null");

                    int sepIdx = -1;
                    for (int i = 0; i < jniResult.length; i++) {
                        if ("---FILES---".equals(jniResult[i])) { sepIdx = i; break; }
                    }
                    if (sepIdx < 0) throw new Exception("Separator not found in JNI result");

                    int batchSize = 10000;
                    int totalBatches = (int) Math.ceil((double) sepIdx / (batchSize * 2));
                    int currentBatch = 0;

                    for (int i = 0; i < sepIdx; i += batchSize * 2) {
                        currentBatch++;
                        int finalCurrentBatch = currentBatch;
                        runOnUiThread(() -> {
                            int pct = (int) (((double) finalCurrentBatch / totalBatches) * 50); // Scale to 50%
                            progressBar.setProgress(pct);
                            tvProgress.setText(String.format("Uploading Secure Indexes (%d/%d)...", finalCurrentBatch, totalBatches));
                        });

                        JSONArray pairs = new JSONArray();
                        int end = Math.min(i + batchSize * 2, sepIdx);
                        for (int j = i; j < end; j += 2) {
                            JSONObject pair = new JSONObject();
                            pair.put("key",   jniResult[j]);
                            pair.put("value", jniResult[j + 1]);
                            pairs.put(pair);
                        }
                        JSONObject body = new JSONObject();
                        body.put("dbName", dbName);
                        body.put("pairs", pairs);
                        NetworkUtils.performPostRequest(baseUrl + "/bulk-save-index_value", body.toString(), token);
                    }

                    runOnUiThread(() -> {
                        tvProgress.setText("Encrypting and Uploading Files...");
                        progressBar.setProgress(75);
                    });

                    List<String> encPaths = new ArrayList<>();
                    for (int i = sepIdx + 1; i < jniResult.length; i++)
                        encPaths.add(jniResult[i]);
                    NetworkUtils.performMultipartRequest(baseUrl + "/upload_files", dbName, encPaths, token);

                    for (String p : encPaths) new File(p).delete();
                    for (FileItem item : selectedFiles) new File(item.localPath).delete();
                    selectedFiles.clear();

                } else {
                    // ---- DELETE mode ----
                    runOnUiThread(() -> tvProgress.setText("Generating Secure Delete Signal..."));
                    
                    String targetId  = etTargetId.getText().toString().trim();
                    int    targetKw  = Integer.parseInt(etTargetKeywords.getText().toString().trim());

                    jniResult = generateDeleteTokens(storagePath, spaceKey, KW_SPACE_SIZE,
                            new String[]{targetId}, new int[]{targetKw});
                    if (jniResult == null) throw new Exception("Native delete token generation returned null");

                    int batchSize = 10000;
                    int totalBatches = (int) Math.ceil((double) jniResult.length / (batchSize * 2));
                    int currentBatch = 0;

                    for (int i = 0; i < jniResult.length; i += batchSize * 2) {
                        currentBatch++;
                        int finalCurrentBatch = currentBatch;
                        runOnUiThread(() -> {
                            int pct = (int) (((double) finalCurrentBatch / totalBatches) * 100); 
                            progressBar.setProgress(pct);
                            tvProgress.setText(String.format("Propagating Delete Signal (%d/%d)...", finalCurrentBatch, totalBatches));
                        });

                        JSONArray pairs = new JSONArray();
                        int end = Math.min(i + batchSize * 2, jniResult.length);
                        for (int j = i; j < end; j += 2) {
                            JSONObject pair = new JSONObject();
                            pair.put("key",   jniResult[j]);
                            pair.put("value", jniResult[j + 1]);
                            pairs.put(pair);
                        }
                        JSONObject body = new JSONObject();
                        body.put("dbName", dbName);
                        body.put("pairs", pairs);
                        NetworkUtils.performPostRequest(baseUrl + "/bulk-save-index_value", body.toString(), token);
                    }
                }

                runOnUiThread(() -> {
                    progressBar.setProgress(100);
                    tvProgress.setText("Update Complete!");
                    setUIEnabled(true);
                    Toast.makeText(this, "Update successful!", Toast.LENGTH_LONG).show();
                    fileAdapter.notifyDataSetChanged();
                    finish();
                });

            } catch (Exception e) {
                Log.e(TAG, "Update failed", e);
                runOnUiThread(() -> {
                    setUIEnabled(true);
                    tvProgress.setText("Update Failed");
                    progressBar.setProgress(0);
                    Toast.makeText(this, "Update failed: " + e.getMessage(), Toast.LENGTH_LONG).show();
                });
            }
        }).start();
    }

    private void setUIEnabled(boolean enabled) {
        btnPerformUpdate.setEnabled(enabled);
        btnPerformUpdate.setAlpha(enabled ? 1.0f : 0.5f);
        btnAddFile.setEnabled(enabled);
        spinnerSpaces.setEnabled(enabled);
        btnModeAdd.setEnabled(enabled);
        btnModeDelete.setEnabled(enabled);
        etTargetId.setEnabled(enabled);
        etTargetKeywords.setEnabled(enabled);
    }

    private void fetchAllSpaces() {
        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        String token = prefs.getString("auth_token", null);
        String ip    = prefs.getString("last_ip", BuildConfig.SERVER_IP);
        String url   = "http://" + ip + ":3000/api/get-spaces";

        new Thread(() -> {
            try {
                String response = NetworkUtils.performGetRequest(url, token);
                JSONObject json = new JSONObject(response);
                JSONArray spaces = json.optJSONArray("spaces");
                runOnUiThread(() -> updateSpaceSpinner(spaces));
            } catch (Exception e) {
                Log.e(TAG, "Error fetching spaces", e);
                runOnUiThread(() -> Toast.makeText(this, "Error fetching spaces", Toast.LENGTH_SHORT).show());
            }
        }).start();
    }

    private void updateSpaceSpinner(JSONArray spaces) {
        List<String> list = new ArrayList<>();
        if (spaces != null) {
            for (int i = 0; i < spaces.length(); i++) {
                try { list.add(spaces.getString(i)); } catch (Exception ignored) {}
            }
        }
        if (list.isEmpty()) {
            list.add("No spaces available");
            tvEmptyMessage.setVisibility(View.VISIBLE);
        } else {
            tvEmptyMessage.setVisibility(View.GONE);
        }
        ArrayAdapter<String> adapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_item, list);
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spinnerSpaces.setAdapter(adapter);
    }

    private String getDbStoragePath(String dbName) {
        File dir = new File(getFilesDir(), dbName);
        if (!dir.exists()) dir.mkdirs();
        return dir.getAbsolutePath();
    }

    private String getFileName(Uri uri) {
        String result = null;
        if ("content".equals(uri.getScheme())) {
            try (Cursor c = getContentResolver().query(uri, null, null, null, null)) {
                if (c != null && c.moveToFirst()) {
                    int idx = c.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                    if (idx >= 0) result = c.getString(idx);
                }
            }
        }
        if (result == null) {
            result = uri.getPath();
            int cut = result != null ? result.lastIndexOf('/') : -1;
            if (cut >= 0) result = result.substring(cut + 1);
        }
        return result;
    }

    private String copyUriToInternalStorage(Uri uri) {
        try {
            String name = getFileName(uri);
            File file = new File(getFilesDir(), name);
            try (InputStream is = getContentResolver().openInputStream(uri);
                 FileOutputStream os = new FileOutputStream(file)) {
                byte[] buf = new byte[8192];
                int n;
                while ((n = is.read(buf)) > 0) os.write(buf, 0, n);
            }
            return file.getAbsolutePath();
        } catch (Exception e) {
            Log.e(TAG, "Copy failed", e);
            return null;
        }
    }

    static class FileItem {
        String name, keyword, localPath;
        FileItem(String n, String k, String p) { name = n; keyword = k; localPath = p; }
    }

    static class FileAdapter extends RecyclerView.Adapter<FileVH> {
        private final List<FileItem> files;
        private final Runnable onChange;
        FileAdapter(List<FileItem> f, Runnable r) { files = f; onChange = r; }

        @NonNull @Override
        public FileVH onCreateViewHolder(@NonNull ViewGroup parent, int type) {
            View v = LayoutInflater.from(parent.getContext()).inflate(R.layout.item_file_selection, parent, false);
            return new FileVH(v);
        }

        @Override public void onBindViewHolder(@NonNull FileVH h, int pos) {
            FileItem item = files.get(pos);
            h.tvFileName.setText(item.name);
            
            // Very important: remove the old TextWatcher before setText 
            // to stop it from changing the previous list item's keyword!
            if (h.currentWatcher != null) {
                h.etKeyword.removeTextChangedListener(h.currentWatcher);
            }
            
            h.etKeyword.setText(item.keyword);
            
            h.currentWatcher = new TextWatcher() {
                @Override public void beforeTextChanged(CharSequence s, int st, int c, int a) {}
                @Override public void onTextChanged(CharSequence s, int st, int b, int c) {}
                @Override public void afterTextChanged(Editable s) { 
                    item.keyword = s.toString(); 
                    onChange.run(); 
                }
            };
            h.etKeyword.addTextChangedListener(h.currentWatcher);
            
            h.btnRemove.setOnClickListener(v -> {
                int p = h.getAdapterPosition();
                if (p != RecyclerView.NO_POSITION) { files.remove(p); notifyItemRemoved(p); onChange.run(); }
            });
        }
        @Override public int getItemCount() { return files.size(); }
    }

    static class FileVH extends RecyclerView.ViewHolder {
        TextView tvFileName; EditText etKeyword; ImageButton btnRemove;
        TextWatcher currentWatcher;
        
        FileVH(View v) {
            super(v);
            tvFileName = v.findViewById(R.id.tvFileName);
            etKeyword  = v.findViewById(R.id.etKeyword);
            btnRemove  = v.findViewById(R.id.btnRemoveFile);
        }
    }
}
