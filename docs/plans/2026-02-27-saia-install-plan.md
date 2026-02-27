# SAIA Installation on SERV00 Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Provide a verified command-line installation guide for SAIA on SERV00 hosting.

**Architecture:** Use standard FreeBSD tools (git, gcc/clang, screen) available on SERV00.

**Tech Stack:** Git, GCC/Clang, Screen/Tmux, Bash.

---

### Task 1: Verify Environment and Dependencies (User Action)

**Files:**
- None

**Step 1: Check for git and compiler**

```bash
git --version
cc --version
```

**Step 2: Install git if missing (Optional - usually pre-installed)**
Note: On SERV00, users cannot install packages. If git is missing, they must request it or use manual download. Assuming git is present.

### Task 2: Clone Repository (User Action)

**Files:**
- Create: `~/saia_install/` (or similar)

**Step 1: Clone the repository**

```bash
cd ~
git clone https://github.com/Ioveo/s.git saia
cd saia
```

### Task 3: Compile Source Code (User Action)

**Files:**
- Modify: None
- Run: Compilation command

**Step 1: Compile using gcc/clang**

```bash
# SERV00 uses FreeBSD, so 'cc' is usually clang. GCC might be 'gcc' or 'gcc10' etc.
# We will use 'cc' as it's the system compiler.
cc *.c -o saia -lpthread -lm -std=c11 -Wall -O2
```

**Step 2: Verify compilation**

```bash
./saia
# Expect: Menu or usage output
```

### Task 4: Setup Background Service (User Action)

**Files:**
- Create: `start_saia.sh`

**Step 1: Create startup script**

```bash
cat << 'EOF' > start_saia.sh
#!/bin/sh
cd ~/saia
# Start screen session named 'saia' in detached mode
screen -dmS saia ./saia
EOF
chmod +x start_saia.sh
```

**Step 2: Add to cron (Optional but recommended)**

```bash
(crontab -l 2>/dev/null; echo "@reboot ~/saia/start_saia.sh") | crontab -
```

### Task 5: Managing the Service (User Action)

**Files:**
- None

**Step 1: Attach to screen**

```bash
screen -r saia
```

**Step 2: Detach from screen**

Press `Ctrl+A` then `D`.

**Step 3: Stop service**

```bash
screen -S saia -X quit
```
