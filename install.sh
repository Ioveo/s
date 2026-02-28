#!/usr/local/bin/bash

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
    printf "${GREEN}[INFO]${NC} %s\n" "$1"
}

warn() {
    printf "${YELLOW}[WARN]${NC} %s\n" "$1"
}

error() {
    printf "${RED}[ERROR]${NC} %s\n" "$1"
    exit 1
}

check_env() {
    log "Checking environment..."
    command -v git >/dev/null 2>&1 || error "Git is not installed."
    command -v screen >/dev/null 2>&1 || error "Screen is not installed. On Serv00, try: pkg install screen"

    # Try to find a working C compiler
    if command -v cc >/dev/null 2>&1; then COMPILER="cc";
    elif command -v gcc >/dev/null 2>&1; then COMPILER="gcc";
    elif command -v clang >/dev/null 2>&1; then COMPILER="clang";
    elif command -v gcc10 >/dev/null 2>&1; then COMPILER="gcc10";
    elif command -v clang10 >/dev/null 2>&1; then COMPILER="clang10";
    else error "No C compiler found."; fi

    log "Using compiler: $COMPILER"

    # Determine extra GCC-only flags
    case "$COMPILER" in
        gcc*) EXTRA_FLAGS="-Wno-unused-but-set-variable" ;;
        *)    EXTRA_FLAGS="" ;;
    esac
}

fix_source_code() {
    log "Applying fixes to source code..."
    cd "$INSTALL_DIR" || error "fix_source_code: cannot cd to $INSTALL_DIR"

    # Fix 1: string_ops.c - strchr uses single quotes
    if [ -f "string_ops.c" ]; then
        # Use perl for in-place edit as it's more portable than sed -i
        perl -pi -e "s/strchr\\(p, \\\"\\|\\\"\\)/strchr(p, '|')/g" string_ops.c
        perl -pi -e "s/strnicmp/strncasecmp/g" string_ops.c
        log "Patched string_ops.c"
    fi

    # Fix 2: saia.h - Add declaration for color_white
    if [ -f "saia.h" ]; then
        if ! grep -q "void color_white(void);" saia.h; then
            # Append before #endif using perl
            perl -pi -e 's/#endif/void color_white(void);\n#endif/' saia.h
            log "Patched saia.h (added color_white)"
        fi

        # Fix 3: saia.h - Fix socket_connect_timeout declaration type mismatch
        perl -pi -e "s/int addrlen/socklen_t addrlen/g" saia.h
        log "Patched saia.h (fixed socket_connect_timeout type)"
    fi

    # Fix 4: network.c - Fix ip_int member access error AND Add missing dns_resolve
    if [ -f "network.c" ]; then
        # Replace the problematic inet_pton line with string copy logic
        perl -pi -e 's/if \(inet_pton\(AF_INET, str, &addr->ip_int\) <= 0\) \{/strncpy(addr->ip, str, sizeof(addr->ip)); if (0) {/' network.c
        log "Patched network.c (fixed ip_int member access)"

        # Check if dns_resolve exists, if not append it
        if ! grep -q "int dns_resolve" network.c; then
            log "Appending missing dns_resolve implementation to network.c..."
            cat << 'EOF' >> network.c

// Missing dns_resolve implementation added by installer
int dns_resolve(const char *hostname, char *ip_buf, size_t size) {
    struct addrinfo hints, *res;
    int err;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if ((err = getaddrinfo(hostname, NULL, &hints, &res)) != 0) {
        return -1;
    }

    struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
    if (!inet_ntop(AF_INET, &(ipv4->sin_addr), ip_buf, size)) {
        freeaddrinfo(res);
        return -1;
    }

    freeaddrinfo(res);
    return 0;
}
EOF
        fi
    fi
}

install_saia() {
    if [ -d "$INSTALL_DIR" ]; then
        log "Directory $INSTALL_DIR exists. Updating..."
        cd "$INSTALL_DIR" || error "Failed to access directory"
        git pull || error "Git pull failed. Try: cd $INSTALL_DIR && git reset --hard HEAD && git pull"
    else
        log "Cloning repository..."
        git clone "$REPO_URL" "$INSTALL_DIR" || error "Git clone failed"
        cd "$INSTALL_DIR" || error "Failed to access directory"
    fi

    fix_source_code

    log "Compiling..."
    cd "$INSTALL_DIR" || error "Failed to access install directory"
    rm -f "$BIN_NAME"

    # Compile (EXTRA_FLAGS is GCC-only; clang does not support -Wno-unused-but-set-variable)
    $COMPILER *.c -o "$BIN_NAME" -lpthread -lm -std=c11 -Wall -O2 \
        -Wno-format -Wno-unused-variable $EXTRA_FLAGS \
        -Wno-implicit-function-declaration

    if [ ! -f "$BIN_NAME" ]; then
        error "Compilation failed. Re-run manually: cd $INSTALL_DIR && $COMPILER *.c -o $BIN_NAME -lpthread -lm -std=c11"
    fi

    chmod +x "$BIN_NAME"
    log "Build successful."
}

create_wrapper() {
    log "Creating management script..."
    cat << EOF > "$INSTALL_DIR/saia_manager.sh"
#!/usr/local/bin/bash
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
        if ! screen -list | grep -q "\$SCREEN_NAME"; then
            \$0 start
            sleep 1
        fi
        \$0 attach
        ;;
esac
EOF
    chmod +x "$INSTALL_DIR/saia_manager.sh"

    # 自动判断 shell 类型，写入对应配置文件
    case "$SHELL" in
        */bash) SHELL_RC="$HOME/.bashrc" ;;
        */zsh)  SHELL_RC="$HOME/.zshrc" ;;
        *)      SHELL_RC="$HOME/.profile" ;;
    esac

    if ! grep -q "alias saia=" "$SHELL_RC" 2>/dev/null; then
        echo "alias saia='$INSTALL_DIR/saia_manager.sh'" >> "$SHELL_RC"
        log "Alias added to $SHELL_RC"
    fi
}

setup_autostart() {
    log "Setting up autostart via crontab..."
    CRON_CMD="@reboot $INSTALL_DIR/saia_manager.sh start"
    if ! (crontab -l 2>/dev/null | grep -v "saia_manager.sh"; echo "$CRON_CMD") | crontab - 2>/dev/null; then
        warn "crontab setup failed. Please add autostart manually via the Serv00 panel."
    fi
}

main() {
    check_env
    install_saia
    create_wrapper
    setup_autostart
    "$INSTALL_DIR/saia_manager.sh" start
    printf "\n${YELLOW}================================================${NC}\n"
    printf "${GREEN}安装完成！程序已在后台运行。${NC}\n"
    printf "${YELLOW}请在终端复制粘贴并回车执行以下命令来呼出菜单：${NC}\n\n"
    printf "    ${GREEN}source %s && saia${NC}\n\n" "$SHELL_RC"
    printf "${YELLOW}以后无论何时，只要在终端输入 ${GREEN}saia${YELLOW} 即可直接打开菜单！${NC}\n"
    printf "${YELLOW}离开菜单按 Ctrl+A 然后按 D 即可保持后台运行。${NC}\n"
    printf "${YELLOW}================================================${NC}\n\n"
}

main
