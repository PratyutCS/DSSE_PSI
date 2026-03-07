#!/bin/bash

# Configuration
SERVER_DIR=$(dirname "$0")
STORAGE_DIR="$SERVER_DIR/storage"
LOG_FILE="$SERVER_DIR/test/verify_suite.log"
DOWNLOADS_DIR="$SERVER_DIR/test/test_downloads"
SERVER_LOG="$SERVER_DIR/server_out.log"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Trap Ctrl+C (SIGINT) to prevent accidental exit
trap 'echo -e "\n${YELLOW}[INFO] Ctrl+C detected. Use the menu to quit/stop.${NC}"' SIGINT

current_pid=""

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

install_dependencies() {
    log_info "Installing dependencies..."
    cd "$SERVER_DIR" || exit
    npm install
    if [ $? -eq 0 ]; then
        log_info "Dependencies installed successfully."
    else
        log_error "Failed to install dependencies."
    fi
}

compile_cpp() {
    log_info "Compiling C++ backend..."
    cd "$SERVER_DIR" || exit
    # Ensure cpp dir exists
    if [ ! -d "cpp" ]; then
        log_error "cpp directory not found!"
        return
    fi
    
    if npm run compile; then
        log_info "Compilation successful."
    else
        log_error "Compilation failed."
    fi
}

clean_data() {
    log_warn "This will DELETE all user data in storage/ and logs. Are you sure? (y/N)"
    read -r response
    if [[ "$response" =~ ^([yY][eE][sS]|[yY])$ ]]; then
        log_info "Cleaning data..."
        
        if [ -d "$STORAGE_DIR" ]; then
            rm -rf "$STORAGE_DIR"/*
            log_info "Storage cleared."
        fi

        rm -f "$SERVER_DIR"/*.log
        if [ -f "$LOG_FILE" ]; then
            rm -f "$LOG_FILE"
        fi
        
        if [ -d "$DOWNLOADS_DIR" ]; then
            rm -rf "$DOWNLOADS_DIR"
        fi

        log_info "Cleanup complete."
    else
        log_info "Cleanup cancelled."
    fi
}

run_tests() {
    log_info "Running Verification Suite..."
    cd "$SERVER_DIR" || exit
    
    if lsof -Pi :3000 -sTCP:LISTEN -t >/dev/null ; then
        log_info "Server appears to be running on port 3000."
    else
        log_warn "Server does not seem to be running on port 3000."
        echo "Do you want to start the server in the background? (y/N)"
        read -r start_srv
        if [[ "$start_srv" =~ ^([yY][eE][sS]|[yY])$ ]]; then
             npm start > "$SERVER_LOG" 2>&1 &
             SERVER_PID=$!
             log_info "Server started (PID: $SERVER_PID). Waiting 5s..."
             sleep 5
             # Verify it's still alive
             if ! ps -p $SERVER_PID > /dev/null; then
                 log_error "Server failed to start. Check $SERVER_LOG"
             fi
        fi
    fi

    node test/verify_suite.js
    
    if [ ! -z "$SERVER_PID" ]; then
        log_info "Killing temporary server (PID: $SERVER_PID)..."
        kill "$SERVER_PID"
    fi
}

start_server_logic() {
    MODE=$1 # "dev" or "prod"
    cd "$SERVER_DIR" || exit

    while true; do
        if [ "$MODE" == "prod" ]; then
             export NODE_ENV=production
        else
             unset NODE_ENV
        fi

        log_info "Starting Server ($MODE) in background..."
        # Start in background, redirect logs
        npm start > "$SERVER_LOG" 2>&1 &
        current_pid=$!
        
        # Wait a moment to ensure it doesn't crash immediately
        sleep 2
        
        if ! ps -p $current_pid > /dev/null; then
            log_error "Server failed to start immediately. Check logs:"
            tail -n 10 "$SERVER_LOG"
            current_pid=""
            break
        fi

        # Control Loop
        while true; do
            echo ""
            echo -e "${BLUE}Server Running (PID: $current_pid)${NC}"
            echo "Logs: $SERVER_LOG"
            echo "1. Restart Server"
            echo "2. Stop & Exit to Menu"
            echo "3. View Live Logs (Ctrl+C to exit logs)"
            read -r -p "Select option: " subchoice

            case $subchoice in
                1)
                    log_info "Restarting..."
                    kill $current_pid
                    wait $current_pid 2>/dev/null
                    break # Breaks inner loop, repeats outer loop (starting server)
                    ;;
                2)
                    log_info "Stopping server..."
                    kill $current_pid
                    wait $current_pid 2>/dev/null
                    current_pid=""
                    return # Exits function, back to main menu
                    ;;
                3)
                    tail -f "$SERVER_LOG"
                    ;;
                *)
                    log_warn "Invalid option"
                    ;;
            esac
        done
    done
}


# Main Menu
while true; do
    echo "----------------------------------------"
    echo -e "${GREEN}PI Server Management CLI${NC}"
    echo "----------------------------------------"
    echo "1. Install Dependencies"
    echo "2. Recompile C++ Backend"
    echo "3. Clean Data & Logs"
    echo "4. Run Verification Tests"
    echo "5. Start Server (Dev)"
    echo "6. Start Server (Prod)"
    echo "q. Quit"
    echo "----------------------------------------"
    read -r -p "Select an option: " choice

    case $choice in
        1) install_dependencies ;;
        2) compile_cpp ;;
        3) clean_data ;;
        4) run_tests ;;
        5) start_server_logic "dev" ;;
        6) start_server_logic "prod" ;;
        q|Q) 
            if [ ! -z "$current_pid" ]; then
                 log_warn "Stopping active server (PID: $current_pid) before quitting..."
                 kill $current_pid 2>/dev/null
            fi
            break 
            ;;
        *) log_error "Invalid option." ;;
    esac
    echo ""
done
