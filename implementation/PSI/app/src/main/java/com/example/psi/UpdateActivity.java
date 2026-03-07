package com.example.psi;

import android.content.Context;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
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
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;
import android.database.Cursor;
import android.provider.OpenableColumns;

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
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class UpdateActivity extends AppCompatActivity {

    static {
        System.loadLibrary("psi");
    }

    private native String[] generateTokens(String storagePath, String[] filePaths, int[] keywords);

    private Spinner spinnerSpaces;
    private Button btnPerformUpdate, btnAddFile;
    private TextView tvEmptyMessage;
    private RecyclerView rvFiles;
    private FileSelectionAdapter adapter;
    private List<FileSelection> selectedFiles = new ArrayList<>();

    private final ExecutorService executor = Executors.newSingleThreadExecutor();
    private final Handler handler = new Handler(Looper.getMainLooper());
    private static final String PREFS_NAME = "psi_prefs";

    private final ActivityResultLauncher<String> filePickerLauncher = registerForActivityResult(
            new ActivityResultContracts.GetMultipleContents(),
            this::handleSelectedFiles
    );

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_update);

        spinnerSpaces = findViewById(R.id.spinnerSpaces);
        btnPerformUpdate = findViewById(R.id.btnPerformUpdate);
        btnAddFile = findViewById(R.id.btnAddFile);
        tvEmptyMessage = findViewById(R.id.tvEmptyMessage);
        rvFiles = findViewById(R.id.rvFiles);

        // Initially disable file adding and update button until DB is selected
        btnAddFile.setEnabled(false);
        btnAddFile.setAlpha(0.5f);
        btnPerformUpdate.setEnabled(false);
        btnPerformUpdate.setAlpha(0.5f);

        rvFiles.setLayoutManager(new LinearLayoutManager(this));
        adapter = new FileSelectionAdapter(selectedFiles);
        rvFiles.setAdapter(adapter);

        TextView toolbarTitle = findViewById(R.id.toolbarTitle);
        toolbarTitle.setText("UPDATE DASHBOARD");

        View btnBack = findViewById(R.id.btnBack);
        btnBack.setVisibility(View.VISIBLE);
        btnBack.setOnClickListener(v -> finish());

        btnAddFile.setOnClickListener(v -> filePickerLauncher.launch("*/*"));
        btnPerformUpdate.setOnClickListener(v -> performUpdate());

        fetchEmptySpaces();
    }

    private void updateSpinner(List<String> spaceList) {
        if (spaceList.isEmpty()) {
            tvEmptyMessage.setVisibility(View.VISIBLE);
            spinnerSpaces.setEnabled(false);
            btnAddFile.setEnabled(false);
            btnAddFile.setAlpha(0.5f);
        } else {
            tvEmptyMessage.setVisibility(View.GONE);
            spinnerSpaces.setEnabled(true);
            
            ArrayAdapter<String> spinnerAdapter = new ArrayAdapter<>(this, 
                    R.layout.item_dropdown_luxury, android.R.id.text1, spaceList);
            spinnerSpaces.setAdapter(spinnerAdapter);

            // Enable file adding once we have spaces to update
            btnAddFile.setEnabled(true);
            btnAddFile.setAlpha(1.0f);
        }
    }

    private void performUpdate() {
        if (selectedFiles.isEmpty()) {
            Toast.makeText(this, "Please select at least one file", Toast.LENGTH_SHORT).show();
            return;
        }

        String dbName = spinnerSpaces.getSelectedItem() != null ? spinnerSpaces.getSelectedItem().toString() : "";
        if (dbName.isEmpty()) {
            Toast.makeText(this, "Please select a space", Toast.LENGTH_SHORT).show();
            return;
        }

        // Validate keywords
        for (FileSelection fs : selectedFiles) {
            if (fs.keyword < 0 || fs.keyword > 100000) {
                Toast.makeText(this, "Invalid keyword for " + fs.name + ". Must be 0-100000", Toast.LENGTH_SHORT).show();
                return;
            }
        }

        setUIEnabled(false);
        Toast.makeText(this, "Generating tokens and updating database...", Toast.LENGTH_SHORT).show();
        
        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        String token = prefs.getString("auth_token", null);
        String ip = prefs.getString("last_ip", BuildConfig.SERVER_IP);
        String baseUrl = "http://" + ip + ":3000/api";

        executor.execute(() -> {
            try {
                String[] paths = new String[selectedFiles.size()];
                int[] keywords = new int[selectedFiles.size()];
                List<String> filePathsList = new ArrayList<>();
                List<File> tempFilesToDelete = new ArrayList<>();
                
                for (int i = 0; i < selectedFiles.size(); i++) {
                    File tempFile = copyUriToTempFile(selectedFiles.get(i).uri, selectedFiles.get(i).name);
                    paths[i] = tempFile.getAbsolutePath();
                    keywords[i] = selectedFiles.get(i).keyword;
                    filePathsList.add(paths[i]);
                    tempFilesToDelete.add(tempFile);
                }
                
                // 1. Create encrypted directory in storage path
                File storageBase = getFilesDir();
                File encryptedDir = new File(storageBase, "encrypted");
                if (!encryptedDir.exists()) encryptedDir.mkdirs();

                // 2. Generate Tokens via JNI
                System.out.println("[UPDATE PSI] tokens and encrypted files generating for DB: " + dbName);
                String storagePath = getDbStoragePath(dbName);
                String[] jniResult = generateTokens(storagePath, paths, keywords);
                System.out.println("[UPDATE PSI] tokens and encrypted files generated");
                
                // jniResult: [u0, e0, ..., uN, eN, path0, path1, ...]
                // result.size() * 2 = 200,000 * 2 = 400,000 for tokens
                // filesCount = paths.length
                
                int tokenCount = 200000;
                int tokenEntries = tokenCount * 2;
                
                // 3. Send tokens in batches to server via bulk endpoint
                System.out.println("[UPDATE PSI] sending tokens to server");
                if (jniResult != null && jniResult.length >= tokenEntries) {
                    int BATCH_SIZE = 5000;
                    
                    for (int batchStart = 0; batchStart < tokenCount; batchStart += BATCH_SIZE) {
                        int batchEnd = Math.min(batchStart + BATCH_SIZE, tokenCount);
                        
                        JSONArray pairsArray = new JSONArray();
                        for (int j = batchStart; j < batchEnd; j++) {
                            JSONObject pair = new JSONObject();
                            pair.put("key", jniResult[j * 2]);
                            pair.put("value", jniResult[j * 2 + 1]);
                            pairsArray.put(pair);
                        }
                        
                        JSONObject body = new JSONObject();
                        body.put("dbName", dbName);
                        body.put("pairs", pairsArray);
                        
                        Log.i("PSI_UPDATE", "Sending token batch " + (batchStart / BATCH_SIZE + 1));
                        NetworkUtils.performPostRequest(baseUrl + "/bulk-save-index_value", body.toString(), token);
                    }
                    Log.i("PSI_UPDATE", "All token batches sent successfully");
                }
                
                // 4. Upload encrypted files to server
                System.out.println("[UPDATE PSI] uploading encrypted files to server");
                List<String> encryptedPathsList = new ArrayList<>();
                if (jniResult != null && jniResult.length > tokenEntries) {
                    for (int i = tokenEntries; i < jniResult.length; i++) {
                        encryptedPathsList.add(jniResult[i]);
                    }
                    NetworkUtils.performMultipartRequest(baseUrl + "/upload_files", dbName, encryptedPathsList, token);
                }
                System.out.println("[UPDATE PSI] encrypted files uploaded to server");
                
                // 5. Cleanup encrypted files and temp files from device
                for (String path : encryptedPathsList) {
                    new File(path).delete();
                }
                for (File tempFile : tempFilesToDelete) {
                    if (tempFile.exists()) tempFile.delete();
                }
                
                handler.post(() -> {
                    setUIEnabled(true);
                    selectedFiles.clear();
                    adapter.notifyDataSetChanged();
                    checkUpdateState();
                    Toast.makeText(this, "Update complete!", Toast.LENGTH_LONG).show();
                });
                
            } catch (Exception e) {
                e.printStackTrace();
                handler.post(() -> {
                    setUIEnabled(true);
                    Toast.makeText(this, "Update failed: " + e.getMessage(), Toast.LENGTH_LONG).show();
                });
            }
        });
    }

    private void setUIEnabled(boolean enabled) {
        float alpha = enabled ? 1.0f : 0.4f;
        spinnerSpaces.setEnabled(enabled);
        spinnerSpaces.setAlpha(alpha);
        btnAddFile.setEnabled(enabled);
        btnAddFile.setAlpha(alpha);
        btnPerformUpdate.setEnabled(enabled);
        btnPerformUpdate.setAlpha(alpha);
        
        // Pass to adapter to disable individual items
        adapter.setEnabled(enabled);
    }

    private File copyUriToTempFile(Uri uri, String name) throws Exception {
        InputStream is = getContentResolver().openInputStream(uri);
        File cacheDir = getExternalCacheDir();
        File tempFile = new File(cacheDir, "temp_" + System.currentTimeMillis() + "_" + name);
        FileOutputStream os = new FileOutputStream(tempFile);
        
        byte[] buffer = new byte[8192];
        int bytesRead;
        while ((bytesRead = is.read(buffer)) != -1) {
            os.write(buffer, 0, bytesRead);
        }
        os.close();
        is.close();
        return tempFile;
    }

    // --- Inner Models & Adapter ---

    private static class FileSelection {
        Uri uri;
        String name;
        int keyword = 0;

        FileSelection(Uri uri, String name) {
            this.uri = uri;
            this.name = name;
        }
    }

    private class FileSelectionAdapter extends RecyclerView.Adapter<FileSelectionAdapter.ViewHolder> {
        private final List<FileSelection> items;
        private boolean isEnabled = true;

        FileSelectionAdapter(List<FileSelection> items) {
            this.items = items;
        }

        void setEnabled(boolean enabled) {
            this.isEnabled = enabled;
            notifyDataSetChanged();
        }

        @NonNull
        @Override
        public ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
            View view = LayoutInflater.from(parent.getContext()).inflate(R.layout.item_file_selection, parent, false);
            return new ViewHolder(view);
        }

        @Override
        public void onBindViewHolder(@NonNull ViewHolder holder, int position) {
            FileSelection fs = items.get(position);
            holder.tvName.setText(fs.name);
            holder.tvPath.setText(fs.uri.toString());

            // Remove previous watcher to avoid multiple triggers on reused view
            if (holder.keywordWatcher != null) {
                holder.etKeyword.removeTextChangedListener(holder.keywordWatcher);
            }

            holder.etKeyword.setText(String.valueOf(fs.keyword));
            
            holder.itemView.setEnabled(isEnabled);
            holder.itemView.setAlpha(isEnabled ? 1.0f : 0.4f);
            holder.etKeyword.setEnabled(isEnabled);
            holder.btnRemove.setEnabled(isEnabled);
            
            holder.keywordWatcher = new TextWatcher() {
                @Override public void beforeTextChanged(CharSequence s, int start, int count, int after) {}
                @Override public void onTextChanged(CharSequence s, int start, int before, int count) {}
                @Override public void afterTextChanged(Editable s) {
                    try {
                        if (s != null && s.length() > 0) {
                            fs.keyword = Integer.parseInt(s.toString());
                        }
                    } catch (Exception ignored) {}
                }
            };
            holder.etKeyword.addTextChangedListener(holder.keywordWatcher);

            holder.btnRemove.setOnClickListener(v -> {
                int pos = holder.getAdapterPosition();
                if (pos != RecyclerView.NO_POSITION) {
                    items.remove(pos);
                    notifyItemRemoved(pos);
                    checkUpdateState();
                }
            });
        }

        @Override
        public int getItemCount() {
            return items.size();
        }

        class ViewHolder extends RecyclerView.ViewHolder {
            TextView tvName, tvPath;
            EditText etKeyword;
            ImageButton btnRemove;
            TextWatcher keywordWatcher;

            ViewHolder(View itemView) {
                super(itemView);
                tvName = itemView.findViewById(R.id.tvFileName);
                tvPath = itemView.findViewById(R.id.tvFilePath);
                etKeyword = itemView.findViewById(R.id.etKeyword);
                btnRemove = itemView.findViewById(R.id.btnRemoveFile);
            }
        }
    }

    private void checkUpdateState() {
        boolean hasFiles = !selectedFiles.isEmpty();
        btnPerformUpdate.setEnabled(hasFiles);
        btnPerformUpdate.setAlpha(hasFiles ? 1.0f : 0.5f);
    }

    private void handleSelectedFiles(List<Uri> uris) {
        if (uris == null || uris.isEmpty()) return;
        for (Uri uri : uris) {
            String name = getFileNameFromUri(uri);
            selectedFiles.add(new FileSelection(uri, name));
        }
        adapter.notifyDataSetChanged();
        checkUpdateState();
    }

    private String getFileNameFromUri(Uri uri) {
        String result = null;
        if (uri.getScheme() != null && uri.getScheme().equals("content")) {
            try (Cursor cursor = getContentResolver().query(uri, null, null, null, null)) {
                if (cursor != null && cursor.moveToFirst()) {
                    int index = cursor.getColumnIndex(android.provider.OpenableColumns.DISPLAY_NAME);
                    if (index != -1) {
                        result = cursor.getString(index);
                    }
                }
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
        if (result == null) {
            result = uri.getPath();
            int cut = result != null ? result.lastIndexOf('/') : -1;
            if (cut != -1) result = result.substring(cut + 1);
        }
        return result;
    }

    private void fetchEmptySpaces() {
        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        String token = prefs.getString("auth_token", null);
        String ip = prefs.getString("last_ip", BuildConfig.SERVER_IP);
        String url = "http://" + ip + ":3000/api/get-spaces?uninitialized=true";

        if (token == null) {
            Toast.makeText(this, "Login required", Toast.LENGTH_SHORT).show();
            finish();
            return;
        }

        executor.execute(() -> {
            try {
                String response = NetworkUtils.performGetRequest(url, token);
                JSONObject jsonResponse = new JSONObject(response);
                JSONArray spacesJson = jsonResponse.optJSONArray("spaces");
                
                List<String> spaceList = new ArrayList<>();
                if (spacesJson != null) {
                    for (int i = 0; i < spacesJson.length(); i++) {
                        spaceList.add(spacesJson.getString(i));
                    }
                }

                handler.post(() -> updateSpinner(spaceList));
            } catch (Exception e) {
                e.printStackTrace();
                handler.post(() -> Toast.makeText(this, "Fetch failed: " + e.getMessage(), Toast.LENGTH_LONG).show());
            }
        });
    }
    private String getDbStoragePath(String dbName) {
        File dbDir = new File(getFilesDir(), dbName);
        if (!dbDir.exists()) dbDir.mkdirs();
        return dbDir.getAbsolutePath();
    }
}
