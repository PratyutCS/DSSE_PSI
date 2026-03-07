const mongoose = require('mongoose');
const dotenv = require('dotenv');
const User = require('../src/models/User');
const Session = require('../src/models/Session');
const DBSpace = require('../src/models/DBSpace');
const fs = require('fs');
const path = require('path');

// Load environment config
dotenv.config({ path: path.resolve(__dirname, '../.env') });

const resetDB = async () => {
    try {
        await mongoose.connect(process.env.MONGO_URI);
        console.log('MongoDB Connected');

        // 1. Clear Database
        console.log('Clearing Database...');
        await User.deleteMany({});
        await Session.deleteMany({});
        await DBSpace.deleteMany({});

        // 2. Clear Storage Folder (Optional but good for clean slate)
        const storageRoot = process.env.STORAGE_ROOT || './storage';
        if (fs.existsSync(storageRoot)) {
            console.log('Clearing Storage Directory...');
            fs.rmSync(storageRoot, { recursive: true, force: true });
            fs.mkdirSync(storageRoot, { recursive: true });
        }

        // 3. Create Static User
        console.log('Creating Static User...');
        const staticUser = await User.create({
            username: 'testuser',
            password: 'password123', // Will be hashed by pre-save hook
            role: 'admin'
        });

        console.log(`User created: ${staticUser.username} (ID: ${staticUser._id})`);

        console.log('Database Reset Complete.');
        process.exit(0);
    } catch (error) {
        console.error('Error resetting DB:', error);
        process.exit(1);
    }
};

resetDB();
