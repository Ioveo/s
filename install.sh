#!/bin/bash

# Configuration
REPO_URL="https://github.com/Ioveo/s.git"
INSTALL_DIR="$HOME/saia"
BIN_NAME="saia"
SERVICE_NAME="saia_service"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1"
    exit 1
}

check_env() {
    log "Checking environment..."
    # Check for git
    if ! command -v git >/dev/null 2>&1; then
        error "Git is not installed. Please install git first."
    fi

    # Check for screen
    if ! command -v screen >/dev/null 2>&1; then
        error "Screen is not installed. Please install screen first."
    fi
    
    # Check for compiler
    if command -v cc >/dev/null 2>&1; then
        COMPILER="cc"
    elif command -v gcc >/dev/null 2>&1; then
        COMPILER="gcc"
    elif command -v clang >/dev/null 2>&1; then
        COMPILER="clang"
    else
        error "No C compiler found (cc, gcc, or clang). Please install a compiler."
    fi
    log "Using compiler: $COMPILER"
}

install_saia() {
    if [ -d "$INSTALL_DIR" ]; then
        log "Directory $INSTALL_DIR exists. Updating..."
        cd "$INSTALL_DIR" || error "Failed to access directory"
        git pull || error "Git pull failed"
    else
        log "Cloning repository..."
        git clone "$REPO_URL" "$INSTALL_DIR" || error "Git clone failed"
        cd "$INSTALL_DIR" || error "Failed to access directory"
    fi

    log "Compiling SAIA..."
    # Clean previous build if any
    rm -f "$BIN_NAME"
    
    # Compile
    $COMPILER *.c -o "$BIN_NAME" -lpthread -lm -std=c11 -Wall -O2
    
    if [ ! -f "$BIN_NAME" ]; then
        error "Compilation finished but binary not found."
    fi
    
    chmod +x "$BIN_NAME"
    log "Build successful."
}

create_wrapper() {
    log "Creating management script..."
    
    # Create the manager script
    cat << EOF > "$INSTALL_DIR/saia_manager.sh"
#!/bin/bash
CMD="\$1"
DIR="$INSTALL_DIR"
BIN="./$BIN_NAME"
SCREEN_NAME="$SERVICE_NAME"

cd "\$DIR" || exit 1

case "\$CMD" in
    start)
        if screen -list | grep -q "\$SCREEN_NAME"; then
            echo "Service is already running."
        else
            # Start in detached screen session
            screen -dmS "\$SCREEN_NAME" "\$BIN"
            echo "Service started in screen session '\$SCREEN_NAME'."
        fi
        ;;
    stop)
        if screen -list | grep -q "\$SCREEN_NAME"; then
            screen -S "\$SCREEN_NAME" -X quit
            echo "Service stopped."
        else
            echo "Service is not running."
        fi
        ;;
    restart)
        \$0 stop
        sleep 2
        \$0 start
        ;;
    status)
        if screen -list | grep -q "\$SCREEN_NAME"; then
            echo "Service is RUNNING (Screen session: \$SCREEN_NAME)."
        else
            echo "Service is STOPPED."
        fi
        ;;
    attach)
        if screen -list | grep -q "\$SCREEN_NAME"; then
            echo "Attaching to service... (Press Ctrl+A, then D to detach)"
            sleep 1
            screen -r "\$SCREEN_NAME"
        else
            echo "Service is not running."
        fi
        ;;
    log)
        if [ -f "audit_report.log" ]; then
            tail -f audit_report.log
        else
            echo "Log file not found."
        fi
        ;;
    *)
        echo "Usage: saia {start|stop|restart|status|attach|log}"
        exit 1
        ;;
esac
EOF
    chmod +x "$INSTALL_DIR/saia_manager.sh"
    
    # Add alias to .bashrc or .zshrc if not present
    SHELL_RC="$HOME/.bashrc"
    [ -f "$HOME/.zshrc" ] && SHELL_RC="$HOME/.zshrc"

    if ! grep -q "alias saia=" "$SHELL_RC"; then
        echo "" >> "$SHELL_RC"
        echo "# SAIA Manager Alias" >> "$SHELL_RC"
        echo "alias saia='$INSTALL_DIR/saia_manager.sh'" >> "$SHELL_RC"
        log "Added 'saia' alias to $SHELL_RC"
    fi
}

setup_autostart() {
    log "Setting up autostart via crontab..."
    CRON_CMD="@reboot $INSTALL_DIR/saia_manager.sh start"
    
    # Check if job already exists
    if ! crontab -l 2>/dev/null | grep -q "$INSTALL_DIR/saia_manager.sh"; then
        (crontab -l 2>/dev/null; echo "$CRON_CMD") | crontab -
        log "Autostart enabled."
    else
        log "Autostart already configured."
    fi
}

main() {
    echo "========================================"
    echo "   SAIA Auto-Installer for SERV00"
    echo "========================================"
    
    check_env
    install_saia
    create_wrapper
    setup_autostart
    
    # Start the service
    "$INSTALL_DIR/saia_manager.sh" start
    
    echo "========================================"
    log "Installation Complete!"
    echo -e "${YELLOW}Usage Instructions:${NC}"
    echo "  1. Run 'source ~/.bashrc' (or reopen terminal) to activate the 'saia' command."
    echo "  2. Use the following commands to manage the service:"
    echo "     $ saia status   # Check service status"
    echo "     $ saia attach   # View the running program (Press Ctrl+A, D to detach)"
    echo "     $ saia stop     # Stop the service"
    echo "     $ saia start    # Start the service"
    echo "========================================"
}

main
