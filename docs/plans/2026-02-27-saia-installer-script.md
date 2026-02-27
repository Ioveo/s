# SAIA One-Line Installer Script Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Create a robust `install.sh` script to automate SAIA deployment on SERV00 via `bash <(curl ...)` command.

**Architecture:** Single bash script handling environment checks, dependency verification, source compilation, service setup via `screen`, and crontab management for persistence.

**Tech Stack:** Bash, GCC/Clang, Git, Screen.

---

### Task 1: Create Installer Script Structure

**Files:**
- Create: `install.sh`

**Step 1: Define script header and variables**

```bash
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
```

### Task 2: Implement Dependency Checks & Compilation

**Files:**
- Modify: `install.sh`

**Step 1: Add environment checks**

```bash
check_env() {
    log "Checking environment..."
    command -v git >/dev/null 2>&1 || error "Git is not installed."
    command -v screen >/dev/null 2>&1 || error "Screen is not installed."
    
    # Check for compiler
    if command -v cc >/dev/null 2>&1; then
        COMPILER="cc"
    elif command -v gcc >/dev/null 2>&1; then
        COMPILER="gcc"
    elif command -v clang >/dev/null 2>&1; then
        COMPILER="clang"
    else
        error "No C compiler found (cc, gcc, or clang)."
    fi
    log "Using compiler: $COMPILER"
}
```

**Step 2: Add installation logic**

```bash
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

    log "Compiling..."
    # Clean previous build if any
    rm -f "$BIN_NAME"
    
    # Compile
    $COMPILER *.c -o "$BIN_NAME" -lpthread -lm -std=c11 -Wall -O2 || error "Compilation failed"
    
    if [ ! -f "$BIN_NAME" ]; then
        error "Compilation finished but binary not found."
    fi
    
    chmod +x "$BIN_NAME"
    log "Build successful."
}
```

### Task 3: Implement Service Management Script

**Files:**
- Modify: `install.sh`

**Step 1: Generate management wrapper**

```bash
create_wrapper() {
    log "Creating management script..."
    
    cat << EOF > "$INSTALL_DIR/saia.sh"
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
        \$0 stop
        sleep 1
        \$0 start
        ;;
    status)
        if screen -list | grep -q "\$SCREEN_NAME"; then
            echo "Service is RUNNING."
        else
            echo "Service is STOPPED."
        fi
        ;;
    log)
        # Assuming log file is audit_report.log or similar based on saia.h
        tail -f audit_report.log
        ;;
    attach)
        screen -r "\$SCREEN_NAME"
        ;;
    *)
        echo "Usage: \$0 {start|stop|restart|status|log|attach}"
        exit 1
        ;;
esac
EOF
    chmod +x "$INSTALL_DIR/saia.sh"
    
    # Create alias/link for easy access if possible, or just instruct user
    # On SERV00, modifying .bashrc is best
    if ! grep -q "alias saia=" ~/.bashrc; then
        echo "alias saia='$INSTALL_DIR/saia.sh'" >> ~/.bashrc
        log "Added 'saia' alias to .bashrc"
    fi
}
```

### Task 4: Implement Auto-Start & Finish

**Files:**
- Modify: `install.sh`

**Step 1: Add crontab entry**

```bash
setup_autostart() {
    log "Setting up autostart..."
    CRON_CMD="@reboot $INSTALL_DIR/saia.sh start"
    
    (crontab -l 2>/dev/null | grep -v "$INSTALL_DIR/saia.sh"; echo "$CRON_CMD") | crontab -
    log "Autostart enabled."
}
```

**Step 2: Main execution block**

```bash
main() {
    check_env
    install_saia
    create_wrapper
    setup_autostart
    
    # Start the service
    "$INSTALL_DIR/saia.sh" start
    
    log "Installation Complete!"
    echo -e "${YELLOW}Usage:${NC}"
    echo "  Run 'source ~/.bashrc' to activate the alias."
    echo "  Then use command 'saia' to manage the service:"
    echo "    saia start   - Start service"
    echo "    saia stop    - Stop service"
    echo "    saia attach  - View running program (Ctrl+A, D to detach)"
    echo "    saia log     - View logs"
}

main
```
