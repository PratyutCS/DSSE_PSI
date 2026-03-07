const express = require('express');
const router = express.Router();
const { protect } = require('../middlewares/authMiddleware');

const { registerUser, loginUser, logoutUser } = require('../controllers/authController');
const { createSpace, deleteSpace, getSpaces } = require('../controllers/spaceController');
const { getIndexValue, saveIndexValue, bulkSaveIndexValue } = require('../controllers/indexController');
const { upload, uploadFiles, getFiles, downloadFile } = require('../controllers/fileController');

// Auth Routes
router.post('/register', registerUser); // Optional but helpful
router.post('/login', loginUser);
router.post('/logout', protect, logoutUser);

// DB Space Routes
router.post('/new', protect, createSpace);
router.delete('/delete', protect, deleteSpace);
router.get('/get-spaces', protect, getSpaces);

// Index Routes (RocksDB)
router.get('/get-index_value', protect, getIndexValue);
router.post('/save-index_value', protect, saveIndexValue);
router.post('/bulk-save-index_value', protect, bulkSaveIndexValue);

// File Routes
router.post('/upload_files', protect, upload.array('files'), uploadFiles);
router.get('/get-files', protect, getFiles);
router.get('/download-file', protect, downloadFile);

module.exports = router;
