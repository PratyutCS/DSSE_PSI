const path = require('path');
const fs = require('fs');

const BASE_URL = 'http://localhost:3000/api';
const LOG_FILE = path.join(__dirname, 'verify_update_filter.log');

function log(message) {
    const timestamp = new Date().toISOString();
    const logMsg = `[${timestamp}] ${message}`;
    console.log(logMsg);
    fs.appendFileSync(LOG_FILE, logMsg + '\n');
}

if (fs.existsSync(LOG_FILE)) {
    fs.unlinkSync(LOG_FILE);
}

async function runTest() {
    try {
        log('--- Starting Update Filter Verification ---');

        const user = 'testuser';
        const pass = 'password123';

        // 1. Login
        log('[1] Logging in...');
        let loginRes = await fetch(`${BASE_URL}/login`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ username: user, password: pass })
        });
        let loginData = await loginRes.json();
        const token = loginData.token;
        if (!token) throw new Error('Auth failed');

        const dbName = 'filter_test_db_' + Date.now();

        // 2. Create Space
        log(`[2] Creating space: ${dbName}`);
        await fetch(`${BASE_URL}/new`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json', 'Authorization': `Bearer ${token}` },
            body: JSON.stringify({ dbName })
        });

        // 3. Verify it shows up in uninitialized list
        log('[3] Verifying space appears in uninitialized list');
        let listRes = await fetch(`${BASE_URL}/get-spaces?uninitialized=true`, {
            method: 'GET',
            headers: { 'Authorization': `Bearer ${token}` }
        });
        let listData = await listRes.json();
        if (listData.spaces.includes(dbName)) {
            log('  SUCCESS: Space found in uninitialized list.');
        } else {
            throw new Error('Space NOT found in uninitialized list');
        }

        // 4. Upload a dummy file
        log('[4] Uploading a dummy file to the space');
        const dummyFile = path.join(__dirname, 'data/dummy.txt');
        if (!fs.existsSync(path.dirname(dummyFile))) fs.mkdirSync(path.dirname(dummyFile), { recursive: true });
        fs.writeFileSync(dummyFile, 'Test Content');

        const formData = new FormData();
        const blob = new Blob([fs.readFileSync(dummyFile)]);
        formData.append('files', blob, 'dummy.txt');

        let uploadRes = await fetch(`${BASE_URL}/upload_files?dbName=${dbName}`, {
            method: 'POST',
            headers: { 'Authorization': `Bearer ${token}` },
            body: formData
        });
        if (uploadRes.status !== 200) throw new Error('Upload failed');
        log('  Upload successful.');

        // 5. Verify it NO LONGER shows up in uninitialized list
        log('[5] Verifying space is REMOVED from uninitialized list');
        listRes = await fetch(`${BASE_URL}/get-spaces?uninitialized=true`, {
            method: 'GET',
            headers: { 'Authorization': `Bearer ${token}` }
        });
        listData = await listRes.json();
        if (!listData.spaces.includes(dbName)) {
            log('  SUCCESS: Space correctly filtered out.');
        } else {
            throw new Error('Space STILL found in uninitialized list after upload');
        }

        // Cleanup
        log('[6] Cleaning up...');
        await fetch(`${BASE_URL}/delete`, {
            method: 'DELETE',
            headers: { 'Content-Type': 'application/json', 'Authorization': `Bearer ${token}` },
            body: JSON.stringify({ dbName })
        });
        log('--- Verification Complete ---');

    } catch (e) {
        log('ERROR: ' + e.message);
        process.exit(1);
    }
}

runTest();
