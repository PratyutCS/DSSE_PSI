const jwt = require('jsonwebtoken');
const User = require('../models/User');
const Session = require('../models/Session');
const bcrypt = require('bcrypt');

const generateToken = (id) => {
    return jwt.sign({ id }, process.env.JWT_SECRET, {
        expiresIn: '30d',
    });
};

const registerUser = async (req, res) => {
    const { username, password } = req.body;

    try {
        const userExists = await User.findOne({ username });

        if (userExists) {
            return res.status(400).json({ message: 'User already exists' });
        }

        const user = await User.create({
            username,
            password,
        });

        if (user) {
            // Auto-login or just success
            res.status(201).json({
                _id: user._id,
                username: user.username,
                message: 'User registered successfully',
            });
        } else {
            res.status(400).json({ message: 'Invalid user data' });
        }
    } catch (error) {
        res.status(500).json({ message: error.message });
    }
};

const loginUser = async (req, res) => {
    const { username, password } = req.body;

    try {
        const user = await User.findOne({ username });

        if (user && (await user.matchPassword(password))) {
            const token = generateToken(user._id);

            // Create Session
            const expiresAt = new Date();
            expiresAt.setDate(expiresAt.getDate() + 30); // 30 days

            await Session.create({
                userId: user._id,
                token: token,
                expiresAt: expiresAt,
            });

            res.json({
                _id: user._id,
                username: user.username,
                token: token,
            });
        } else {
            res.status(401).json({ message: 'Invalid username or password' });
        }
    } catch (error) {
        console.error(error);
        res.status(500).json({ message: 'Server Error' });
    }
};

const logoutUser = async (req, res) => {
    try {
        // We expect the middleware 'protect' to have run, so req.token or we can extract it.
        // But 'protect' middleware puts 'user' on req, it doesn't explicitly pass the raw token string easily 
        // unless we modify middleware or re-parse header.
        // Best practice: Middleware attaches token to req.

        let token = req.headers.authorization?.split(' ')[1];

        if (!token) {
            return res.status(400).json({ message: 'No token provided' });
        }

        await Session.deleteOne({ token: token });
        res.json({ message: 'Logged out successfully' });
    } catch (error) {
        console.error(error);
        res.status(500).json({ message: 'Server Error during logout' });
    }
};

module.exports = { registerUser, loginUser, logoutUser };
