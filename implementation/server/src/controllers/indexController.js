const rocksdbService = require('../services/rocksdbService');
const DBSpace = require('../models/DBSpace');
const fs = require('fs');
const path = require('path');

// Helper to resolve dbPath from request
const getDbPath = async (user, dbName) => {
    const space = await DBSpace.findOne({ owner: user._id, dbName: dbName });
    if (!space) throw new Error('Database Space not found');
    return space.path;
}

const getIndexValue = async (req, res) => {
    // mapped to Search functionality
    // Query: ?dbName=x&keyword_token=...&state_token=...&count=...
    const { dbName, keyword_token, state_token, count } = req.query;

    if (!dbName || !keyword_token || !state_token || !count) {
        return res.status(400).json({ message: 'Missing required parameters: dbName, keyword_token, state_token, count' });
    }

    try {
        const dbPath = await getDbPath(req.user, dbName);

        // Ensure DB dir exists passed to C++ which expects it (or creates if rocksdb logic allows, code has create_if_missing=true)
        const results = await rocksdbService.searchIndex(dbPath, keyword_token, state_token, count);

        res.json({ results });
    } catch (error) {
        console.error(error);
        res.status(500).json({ message: error.message || 'Error searching index' });
    }
};

const saveIndexValue = async (req, res) => {
    // mapped to Update functionality
    // Body: { dbName, key, value }
    const { dbName, key, value } = req.body;

    if (!dbName || !key || !value) {
        return res.status(400).json({ message: 'Missing required parameters: dbName, key, value' });
    }

    try {
        const dbPath = await getDbPath(req.user, dbName);

        await rocksdbService.updateIndex(dbPath, key, value);
        res.json({ message: 'Value saved successfully' });
    } catch (error) {
        console.error(error);
        res.status(500).json({ message: error.message || 'Error saving value' });
    }
};

const bulkSaveIndexValue = async (req, res) => {
    // mapped to Batch Update functionality
    // Body: { dbName, pairs: [{key, value}, ...] }
    const { dbName, pairs } = req.body;

    if (!dbName || !pairs || !Array.isArray(pairs) || pairs.length === 0) {
        return res.status(400).json({ message: 'Missing required parameters: dbName, pairs (array of {key, value})' });
    }

    try {
        const dbPath = await getDbPath(req.user, dbName);

        const result = await rocksdbService.bulkUpdateIndex(dbPath, pairs);
        console.log(`[BulkUpdate] ${pairs.length} entries saved to ${dbName}: ${result}`);
        res.json({ message: `Batch update successful: ${pairs.length} entries`, result });
    } catch (error) {
        console.error(error);
        res.status(500).json({ message: error.message || 'Error in bulk update' });
    }
};

module.exports = { getIndexValue, saveIndexValue, bulkSaveIndexValue };
