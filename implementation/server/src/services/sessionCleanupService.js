const Session = require('../models/Session');

const cleanupExpiredSessions = async () => {
    try {
        const result = await Session.deleteMany({ expiresAt: { $lt: new Date() } });
        if (result.deletedCount > 0) {
            console.log(`[Cleanup] Removed ${result.deletedCount} expired sessions.`);
        }
    } catch (error) {
        console.error('[Cleanup] Error removing expired sessions:', error);
    }
};

const startCleanupLoop = (intervalMs = 60000) => {
    // Run immediately on start
    cleanupExpiredSessions();

    // Set interval
    setInterval(cleanupExpiredSessions, intervalMs);
    console.log(`[Cleanup] Session cleanup loop started (Interval: ${intervalMs}ms)`);
};

module.exports = { startCleanupLoop };
