const fs = require('fs');
const path = require('path');
const multer = require('multer');
const DBSpace = require('../models/DBSpace');

// We need a custom storage engine or dynamic destination for multer
// but standard multer usage requires middleware setup in routes.
// We'll define a configured multer instance or helper function here to be used in routes,
// OR we can handle the stream manually if we want extreme flexibility, but let's stick to multer.
// Challenge: Routes need to determine the 'path' based on user request.
// Standard trick: Pass dbName in query/body, but multer runs BEFORE controller body is fully parsed sometimes
// if strictly multipart. However, field order matters.
// EASIER: Assume 'dbName' is passed as a query param or checking headers/part of URL.
// Let's assume passed as Query param `?dbName=xyz` for simplicity in multer destination.

const storage = multer.diskStorage({
    destination: async function (req, file, cb) {
        // We need to resolve where to save.
        // We need the user (from auth middleware) and dbName (from query/body).
        // Note: Auth middleware MUST run before multer.
        const user = req.user;
        const dbName = req.query.dbName || req.body.dbName;

        if (!dbName) {
            return cb(new Error('dbName is required for upload'), null);
        }

        // Verify Space ownership
        try {
            const space = await DBSpace.findOne({ owner: user._id, dbName: dbName });
            if (!space) {
                return cb(new Error('Space not found or access denied'), null);
            }
            cb(null, space.path);
        } catch (e) {
            cb(e, null);
        }
    },
    filename: function (req, file, cb) {
        // Preserve original name or unique it
        cb(null, file.originalname);
    }
});

const upload = multer({ storage: storage });

const uploadFiles = (req, res) => {
    if (!req.file && !req.files) {
        return res.status(400).json({ message: 'No files uploaded' });
    }
    // Metadata about uploaded file
    res.json({
        message: 'File uploaded successfully',
        file: req.file,
    });
};

const getFiles = async (req, res) => {
    const { dbName } = req.query;
    const user = req.user;

    if (!dbName) {
        return res.status(400).json({ message: 'dbName is required' });
    }

    try {
        const space = await DBSpace.findOne({ owner: user._id, dbName: dbName });
        if (!space) {
            return res.status(404).json({ message: 'Space not found' });
        }

        const files = fs.readdirSync(space.path);
        // Simple list
        res.json({
            dbName,
            files: files,
        });
    } catch (error) {
        console.error(error);
        res.status(500).json({ message: 'Error reading files' });
    }
};

const downloadFile = async (req, res) => {
    const { dbName, fileId } = req.query;
    const user = req.user;

    if (!dbName || !fileId) {
        return res.status(400).json({ message: 'dbName and fileId (e.g. ID0) are required' });
    }

    try {
        const space = await DBSpace.findOne({ owner: user._id, dbName: dbName });
        if (!space) {
            return res.status(404).json({ message: 'Space not found' });
        }

        const files = fs.readdirSync(space.path);
        // Find a file that matches fileId exactly OR matches fileId + extension
        const actualFileName = files.find(f => f.startsWith(fileId + ".") || f === fileId);

        if (!actualFileName) {
            console.log(`[File] Not found matching: ${fileId} in ${space.path}`);
            return res.status(404).json({ message: 'File not found' });
        }

        const filePath = path.join(space.path, actualFileName);

        // Security check: ensure filePath is within space.path
        if (!filePath.startsWith(space.path)) {
            return res.status(403).json({ message: 'Access denied' });
        }

        res.download(filePath, actualFileName, (err) => {
            if (err) {
                console.error('Download error:', err);
                // Response might have partially sent, so checks are limited here
                if (!res.headersSent) {
                    res.status(500).json({ message: 'Error downloading file' });
                }
            }
        });

    } catch (error) {
        console.error(error);
        res.status(500).json({ message: 'Server Error downloading file' });
    }
};

module.exports = { upload, uploadFiles, getFiles, downloadFile };
