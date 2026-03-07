// using global fetch available in Node 18+
const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

const BASE_URL = 'http://localhost:3000/api';
const LOG_FILE = path.join(__dirname, 'verify_suite.log');

// Helper for logger
function log(message) {
    const timestamp = new Date().toISOString();
    const logMsg = `[${timestamp}] ${message}`;
    console.log(logMsg);
    fs.appendFileSync(LOG_FILE, logMsg + '\n');
}

// Clear old log file
if (fs.existsSync(LOG_FILE)) {
    fs.unlinkSync(LOG_FILE);
}

// Helper for pause
const sleep = (ms) => new Promise(resolve => setTimeout(resolve, ms));

async function runTest() {
    try {
        log('--- Starting Merged Verification Suite ---');

        const BaseRegUser = 'testuser';
        const BaseRegPass = 'password123';

        // -------------------------------------------------------------------------
        // 1. Auth: Login
        // -------------------------------------------------------------------------
        log('\n[1] Logging in...');
        let loginRes = await fetch(`${BASE_URL}/login`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ username: BaseRegUser, password: BaseRegPass })
        });

        let loginData = await loginRes.json();

        // Handle registration if needed (resiliency)
        if (loginRes.status !== 200 || !loginData.token) {
            log('Login failed, attempting registration...');
            await fetch(`${BASE_URL}/register`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ username: BaseRegUser, password: BaseRegPass })
            });
            // Retry login
            loginRes = await fetch(`${BASE_URL}/login`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ username: BaseRegUser, password: BaseRegPass })
            });
            loginData = await loginRes.json();
        }

        if (!loginData.token) throw new Error('Authentication Failed');
        const token = loginData.token;
        const userId = loginData._id;
        log(`Login Success. User ID: ${userId}`);


        // -------------------------------------------------------------------------
        // 2. Multi-Database Scenario (RocksDB Operations)
        // -------------------------------------------------------------------------
        log('\n[2] Starting Multi-Database Scenario (RocksDB Operations)...');
        const dbNames_Rocks = ['test_db_alpha', 'test_db_beta'];

        for (const dbName of dbNames_Rocks) {
            log(`\n  > [${dbName}] Operations:`);

            // A. Create Space
            await fetch(`${BASE_URL}/delete`, { // Cleanup first
                method: 'DELETE',
                headers: {
                    'Content-Type': 'application/json',
                    'Authorization': `Bearer ${token}`
                },
                body: JSON.stringify({ dbName })
            });

            const spaceRes = await fetch(`${BASE_URL}/new`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                    'Authorization': `Bearer ${token}`
                },
                body: JSON.stringify({ dbName })
            });
            log(`    Create Status: ${spaceRes.status}`);

            // B. Save Index Value (RocksDB Update)
            const uniqueVal = `val_${dbName}`;
            const kvRes = await fetch(`${BASE_URL}/save-index_value`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                    'Authorization': `Bearer ${token}`
                },
                body: JSON.stringify({
                    dbName: dbName,
                    key: 'hello',
                    value: uniqueVal
                })
            });
            log(`    Save (RocksDB) Status: ${kvRes.status}`);
        }

        // -------------------------------------------------------------------------
        // 3. File Upload Scenario + Index Integration
        // -------------------------------------------------------------------------
        const dbName_Files = 'test_db_files';
        log(`\n[3] Starting File Upload Scenario (${dbName_Files})...`);

        // Setup Space
        await fetch(`${BASE_URL}/delete`, { // Cleanup
            method: 'DELETE',
            headers: { 'Content-Type': 'application/json', 'Authorization': `Bearer ${token}` },
            body: JSON.stringify({ dbName: dbName_Files })
        });
        await fetch(`${BASE_URL}/new`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json', 'Authorization': `Bearer ${token}` },
            body: JSON.stringify({ dbName: dbName_Files })
        });
        log(`  Space ${dbName_Files} created.`);

        // Upload Files
        log('  Uploading Files...');
        const file1Path = path.join(__dirname, 'data/file1.txt');
        const file2Path = path.join(__dirname, 'data/file2.txt');

        // Ensure dummy files exist
        if (!fs.existsSync(path.dirname(file1Path))) {
            fs.mkdirSync(path.dirname(file1Path), { recursive: true });
        }
        if (!fs.existsSync(file1Path)) fs.writeFileSync(file1Path, 'Dummy Content 1');
        if (!fs.existsSync(file2Path)) fs.writeFileSync(file2Path, 'Dummy Content 2');

        const file1Content = fs.readFileSync(file1Path);
        const file2Content = fs.readFileSync(file2Path);

        const formData = new FormData();
        const blob1 = new Blob([file1Content]);
        const blob2 = new Blob([file2Content]);

        formData.append('files', blob1, 'file1.txt');
        formData.append('files', blob2, 'file2.txt');

        const uploadRes = await fetch(`${BASE_URL}/upload_files?dbName=${dbName_Files}`, {
            method: 'POST',
            headers: { 'Authorization': `Bearer ${token}` },
            body: formData
        });
        const uploadData = await uploadRes.json();
        log(`  Upload Status: ${uploadRes.status} (${uploadData.message})`);

        // --- NEW: Save and Get Index Value for Uploaded Files ---
        log('  Testing Index Integration for Uploaded Files...');
        // Save
        const indexKey = 'file1.txt';
        const indexVal = 'metadata_for_file1';
        log(`    Saving Index: Key=${indexKey}, Value=${indexVal}`);
        const saveIdxRes = await fetch(`${BASE_URL}/save-index_value`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'Authorization': `Bearer ${token}`
            },
            body: JSON.stringify({
                dbName: dbName_Files,
                key: indexKey,
                value: indexVal
            })
        });
        log(`    Save Index Status: ${saveIdxRes.status}`);

        // -------------------------------------------------------------

        // Retrieve List
        const listRes = await fetch(`${BASE_URL}/get-files?dbName=${dbName_Files}`, {
            method: 'GET',
            headers: { 'Authorization': `Bearer ${token}` }
        });
        const listData = await listRes.json();
        log(`  Files Retrieved: ${JSON.stringify(listData.files)}`);

        // --- NEW: Show stored location using ls ---
        log('\n  [VERIFICATION] Checking filesystem storage for this user/db...');
        const storagePath = path.resolve(__dirname, '../storage', userId, dbName_Files);
        log(`  Target Path: ${storagePath}`);

        try {
            if (fs.existsSync(storagePath)) {
                const lsOutput = execSync(`ls -l "${storagePath}"`, { encoding: 'utf-8' });
                log('  Command: ls -l <path>');
                log('  Output:\n' + lsOutput);
            } else {
                log('  ERROR: Path does not exist on disk!');
            }
        } catch (e) {
            log(`  Error running ls: ${e.message}`);
        }
        // ------------------------------------------

        // --- NEW: Download Verification ---
        log('\n  [VERIFICATION] Testing Secure File Download...');
        const downloadDir = path.join(__dirname, 'test_downloads');
        if (!fs.existsSync(downloadDir)) fs.mkdirSync(downloadDir, { recursive: true });

        const fileToDownload = 'file1.txt';
        const downloadParams = new URLSearchParams({
            dbName: dbName_Files,
            fileName: fileToDownload
        });

        // Test Authentication & Download
        const downloadRes = await fetch(`${BASE_URL}/download-file?${downloadParams}`, {
            method: 'GET',
            headers: { 'Authorization': `Bearer ${token}` }
        });

        if (downloadRes.status === 200) {
            const destPath = path.join(downloadDir, `downloaded_${fileToDownload}`);
            const arrayBuffer = await downloadRes.arrayBuffer();
            const buffer = Buffer.from(arrayBuffer);
            fs.writeFileSync(destPath, buffer);

            log(`    Download Success: Saved to ${destPath}`);

            // Content Check
            const originalContent = fs.readFileSync(file1Path, 'utf-8'); // file1Path from earlier
            const downloadedContent = fs.readFileSync(destPath, 'utf-8');
            if (originalContent === downloadedContent) {
                log('    SUCCESS: Downloaded file content matches original.');
            } else {
                log('    FAILURE: Content mismatch!');
            }

            // LS Check on local download folder
            const lsDl = execSync(`ls -l "${downloadDir}"`, { encoding: 'utf-8' });
            log('    Local Download Dir Content:\n' + lsDl);

        } else {
            const errBody = await downloadRes.json();
            log(`    FAILURE: Download status ${downloadRes.status} - ${errBody.message}`);
        }
        // ------------------------------------------

        // -------------------------------------------------------------------------
        // 4. Visualization & Deletion
        // -------------------------------------------------------------------------
        log('\n[4] Hierarchy Visualization & Deletion...');

        log('\n--- STORAGE HIERARCHY (Before Deletion) ---');
        try {
            const output = execSync('ls -R storage', { encoding: 'utf-8' });
            log(output);
        } catch (e) {
            log('Visualization Error: ' + e.message);
        }
        log('-------------------------------------------\n');

        log('Pausing for 3 seconds before deletion...');
        await sleep(3000);

        // Delete All Spaces Created
        const allTestDBs = [...dbNames_Rocks, dbName_Files];

        for (const db of allTestDBs) {
            log(`  Deleting Space: ${db}...`);
            const delRes = await fetch(`${BASE_URL}/delete`, {
                method: 'DELETE',
                headers: {
                    'Content-Type': 'application/json',
                    'Authorization': `Bearer ${token}`
                },
                body: JSON.stringify({ dbName: db })
            });
            log(`    API Status: ${delRes.status}`);

            // Verify FS
            const checkPath = path.join('./storage', userId, db);
            if (fs.existsSync(checkPath)) {
                log(`    FAILURE: Folder ${checkPath} STILL EXISTS.`);
            } else {
                log(`    SUCCESS: Folder ${checkPath} was deleted.`);
            }
        }

        log('\n--- STORAGE HIERARCHY (After Deletion) ---');
        try {
            const outputAfter = execSync(`ls -R storage/${userId} || echo "User folder empty or gone"`, { encoding: 'utf-8' });
            log(outputAfter);
        } catch (e) {
            log('(User folder likely empty/gone)');
        }
        log('------------------------------------------\n');


        // -------------------------------------------------------------------------
        // 5. Logout Verification
        // -------------------------------------------------------------------------
        log('[5] Logout Verification...');

        // Pre-check
        const preCheck = await fetch(`${BASE_URL}/get-files?dbName=test_db_files`, {
            method: 'GET',
            headers: { 'Authorization': `Bearer ${token}` }
        });
        if (preCheck.status === 401) log('  FAILURE: Token invalid before logout.');
        else log('  Token Valid (Pre-Logout).');

        // Logout
        await fetch(`${BASE_URL}/logout`, {
            method: 'POST',
            headers: { 'Authorization': `Bearer ${token}` }
        });
        log('  Logged Out.');

        // Post-check
        const postCheck = await fetch(`${BASE_URL}/get-files?dbName=test_db_files`, {
            method: 'GET',
            headers: { 'Authorization': `Bearer ${token}` }
        });

        if (postCheck.status === 401) {
            log('  SUCCESS: Access denied after logout (401).');
        } else {
            log(`  FAILURE: Still able to access (Status: ${postCheck.status}).`);
        }

        log('\n--- Full Merged Verification Suite Complete ---');
        log(`Log saved to: ${LOG_FILE}`);

    } catch (error) {
        log('Test Suite Failed: ' + error);
        console.error(error);
    }
}

runTest();
