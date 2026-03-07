const app = require('./app');
const connectDB = require('./config/db');
const { startCleanupLoop } = require('./services/sessionCleanupService');

// Connect Database
connectDB();

// Start Background Jobs
startCleanupLoop(60000); // Check every minute

const PORT = process.env.PORT || 3000;

app.listen(PORT, () => {
    console.log(`Server running on port ${PORT}`);
});
