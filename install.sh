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
    command -v git >/dev/null 2>&1 || error "Git is not installed."
    command -v screen >/dev/null 2>&1 || error "Screen is not installed."
    
    # Try to find a working C compiler
    if command -v cc >/dev/null 2>&1; then COMPILER="cc";
    elif command -v gcc >/dev/null 2>&1; then COMPILER="gcc";
    elif command -v clang >/dev/null 2>&1; then COMPILER="clang";
    elif command -v gcc10 >/dev/null 2>&1; then COMPILER="gcc10";
    elif command -v clang10 >/dev/null 2>&1; then COMPILER="clang10";
    else error "No C compiler found."; fi
    
    log "Using compiler: $COMPILER"
}

fix_source_code() {
    log "Applying fixes to source code..."
    
    # Fix 1: string_ops.c - strchr uses single quotes for char, not double quotes for string
    if [ -f "string_ops.c" ]; then
        sed -i.bak "s/strchr(p, \"|\")/strchr(p, '|')/g" string_ops.c
        # Fix 2: string_ops.c - strnicmp is Windows specific, use strncasecmp on Unix
        sed -i.bak "s/strnicmp/strncasecmp/g" string_ops.c
        log "Patched string_ops.c"
    fi

    # Fix 3: utils.c - Format string warnings (optional but good practice)
    # This is complex to regex safely, skipping unless critical error occurs.
    # The strchr/strnicmp were the main blockers.
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

    # Apply fixes before compiling
    fix_source_code

    log "Compiling..."
    rm -f "$BIN_NAME"
    
    # Attempt compilation
    $COMPILER *.c -o "$BIN_NAME" -lpthread -lm -std=c11 -Wall -O2 -Wno-format
    
    if [ ! -f "$BIN_NAME" ]; then
        error "Compilation failed. Check output above."
    fi
    
    chmod +x "$BIN_NAME"
    log "Build successful."
}

create_wrapper() {
    log "Creating management script..."
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
            screen -dmS "\$SCREEN_NAME" "\$BIN"
            echo "Service started."
        fi
        ;;
    stop)
        screen -S "\$SCREEN_NAME" -X quit
        echo "Service stopped."
        ;;
    restart)
        \$0 stop; sleep 1; \$0 start
        ;;
    status)
        screen -list | grep -q "\$SCREEN_NAME" && echo "RUNNING" || echo "STOPPED"
        ;;
    attach)
        screen -r "\$SCREEN_NAME"
        ;;
    *)
        echo "Usage: saia {start|stop|restart|status|attach}"
        exit 1
        ;;
esac
EOF
    chmod +x "$INSTALL_DIR/saia_manager.sh"
    
    if ! grep -q "alias saia=" ~/.bashrc; then
        echo "alias saia='$INSTALL_DIR/saia_manager.sh'" >> ~/.bashrc
    fi
}

setup_autostart() {
    log "Setting up autostart..."
    CRON_CMD="@reboot $INSTALL_DIR/saia_manager.sh start"
    (crontab -l 2>/dev/null | grep -v "saia_manager.sh"; echo "$CRON_CMD") | crontab -
}

main() {
    check_env
    install_saia
    create_wrapper
    setup_autostart
    "$INSTALL_DIR/saia_manager.sh" start
    log "Installation Complete! Type 'saia attach' to see the menu."
}

main
