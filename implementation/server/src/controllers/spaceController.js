const fs = require('fs');
const path = require('path');
const DBSpace = require('../models/DBSpace');

const createSpace = async (req, res) => {
    const { dbName } = req.body;
    const user = req.user;

    console.log(`[CreateSpace] Request body: ${JSON.stringify(req.body)}`);

    if (!dbName) {
        console.warn(`[CreateSpace] Missing dbName`);
        return res.status(400).json({ message: 'dbName is required' });
    }

    try {
        // Check for duplicate in DB
        const existingSpace = await DBSpace.findOne({ owner: user._id, dbName: dbName });
        if (existingSpace) {
            console.warn(`[CreateSpace] Space already exists: ${dbName}`);
            return res.status(400).json({ message: 'Space with this name already exists' });
        }

        // Define Path
        const spacePath = path.join(process.env.STORAGE_ROOT || './storage', user._id.toString(), dbName);

        // Create Directory
        if (fs.existsSync(spacePath)) {
            // Edge case where FS has it but DB doesn't? Or just safe check.
            // If we proceed we might overwrite or just claim it.
            // Let's error nicely.
            // Actually strictly, if it's not in DB, we could claim it, but let's be safe.
            // return res.status(400).json({ message: 'Directory already exists for this space' });
        }

        // Create Isolated Directories
        fs.mkdirSync(path.join(spacePath, 'index'), { recursive: true });
        fs.mkdirSync(path.join(spacePath, 'files'), { recursive: true });

        // Save metadata
        const space = await DBSpace.create({
            dbName,
            owner: user._id,
            path: spacePath,
        });

        res.status(201).json({
            message: 'Database space created successfully',
            space,
        });

    } catch (error) {
        console.error(error);
        res.status(500).json({ message: 'Server Error creating space' });
    }
};

const deleteSpace = async (req, res) => {
    const { dbName, dbId } = req.body;
    const user = req.user;

    try {
        let space;
        if (dbId) {
            space = await DBSpace.findOne({ _id: dbId, owner: user._id });
        } else if (dbName) {
            space = await DBSpace.findOne({ dbName: dbName, owner: user._id });
        }

        if (!space) {
            return res.status(404).json({ message: 'Space not found' });
        }

        // Delete Folder
        if (fs.existsSync(space.path)) {
            fs.rmSync(space.path, { recursive: true, force: true });
        }

        // Remove from DB
        await DBSpace.deleteOne({ _id: space._id });

        res.json({ message: 'Database space deleted successfully' });

    } catch (error) {
        console.error(error);
        res.status(500).json({ message: 'Server Error deleting space' });
    }
};

const getSpaces = async (req, res) => {
    const user = req.user;
    const { uninitialized } = req.query;

    try {
        const spaces = await DBSpace.find({ owner: user._id });

        const spaceNames = spaces.map(s => s.dbName);
        res.json({ spaces: spaceNames });
    } catch (error) {
        console.error(error);
        res.status(500).json({ message: 'Server Error fetching spaces' });
    }
};

const getSpaceInfo = async (req, res) => {
    const user = req.user;
    const { dbName } = req.query;

    if (!dbName) return res.status(400).json({ message: 'dbName required' });

    try {
        const space = await DBSpace.findOne({ owner: user._id, dbName: dbName });
        if (!space) return res.status(404).json({ message: 'Space not found' });

        res.json({
            dbName: space.dbName,
            fileCount: space.fileCount || 0
        });
    } catch (error) {
        console.error(error);
        res.status(500).json({ message: 'Error fetching space info' });
    }
};

module.exports = { createSpace, deleteSpace, getSpaces, getSpaceInfo };
