const { spawn } = require('child_process');
const path = require('path');

// Path to compiled executable
const EXE_PATH = path.resolve(__dirname, '../../cpp/dsse_server');

// DB Path resolver helper - Assuming we just need to point to the user's space
// But the service calls (getValue/putValue) might need to know WHICH space.
// The original prompt API passed it implicitly or we need to update the signature.
// original: rocksdbService.getValue(key) -> Wait, how did it know which DB?
// The previous implementation was: `rocksdbAddon.get(key)`. The mock used a global map.
// The PROMPT requirement "File system folders per 'database space'" implies multitenancy.
// BUT the original rocksdbService just had `getValue(key)`.
// We need to update the service signature to accept `dbPath`.

const runCommand = (dbPath, opCode, args) => {
    return new Promise((resolve, reject) => {
        // Prepare arguments: dbPath, opCode, ...args
        const cmdArgs = [dbPath, opCode, ...args];

        const proc = spawn(EXE_PATH, cmdArgs);

        let stdoutData = '';
        let stderrData = '';

        proc.stdout.on('data', (data) => {
            stdoutData += data.toString();
        });

        proc.stderr.on('data', (data) => {
            stderrData += data.toString();
        });

        proc.on('close', (code) => {
            if (code !== 0) {
                return reject(new Error(`DSSE Process exited with code ${code}: ${stderrData}`));
            }
            resolve(stdoutData.trim());
        });

        proc.on('error', (err) => {
            reject(err);
        });
    });
};

const updateIndex = async (dbPath, key, value) => {
    // OpCode 0: Update
    // Wrapper to call C++: ./dsse_server <dbPath> 0 <key> <value>
    return await runCommand(dbPath, '0', [key, value]);
};

const searchIndex = async (dbPath, keywordToken, stateToken, count) => {
    // OpCode 1: Search
    // Wrapper to call C++: ./dsse_server <dbPath> 1 <keywordToken> <stateToken> <count>
    const output = await runCommand(dbPath, '1', [keywordToken, stateToken, count]);

    // Parse Output: "RESULT:some_val\nRESULT:another"
    const results = output.split('\n')
        .filter(line => line.startsWith('RESULT:'))
        .map(line => line.replace('RESULT:', '').trim());

    return results;
};

const bulkUpdateIndex = async (dbPath, pairs) => {
    // OpCode 2: Batch Update
    // Feeds key\tvalue pairs via stdin to a single C++ process
    return new Promise((resolve, reject) => {
        const cmdArgs = [dbPath, '2'];
        const proc = spawn(EXE_PATH, cmdArgs);

        let stdoutData = '';
        let stderrData = '';

        proc.stdout.on('data', (data) => {
            stdoutData += data.toString();
        });

        proc.stderr.on('data', (data) => {
            stderrData += data.toString();
        });

        proc.on('close', (code) => {
            if (code !== 0) {
                return reject(new Error(`DSSE Batch Process exited with code ${code}: ${stderrData}`));
            }
            resolve(stdoutData.trim());
        });

        proc.on('error', (err) => {
            reject(err);
        });

        // Write all pairs to stdin as tab-separated lines
        for (const pair of pairs) {
            proc.stdin.write(`${pair.key}\t${pair.value}\n`);
        }
        proc.stdin.end();
    });
};

module.exports = {
    updateIndex,
    searchIndex,
    bulkUpdateIndex
};
