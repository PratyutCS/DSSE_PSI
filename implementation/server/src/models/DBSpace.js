const mongoose = require('mongoose');

const dbSpaceSchema = new mongoose.Schema({
    dbName: {
        type: String,
        required: true,
    },
    owner: {
        type: mongoose.Schema.Types.ObjectId,
        ref: 'User',
        required: true,
    },
    path: {
        type: String,
        required: true,
    },
}, { timestamps: true });

// Compound index to ensure unique dbName per user
dbSpaceSchema.index({ owner: 1, dbName: 1 }, { unique: true });

const DBSpace = mongoose.model('DBSpace', dbSpaceSchema);
module.exports = DBSpace;
