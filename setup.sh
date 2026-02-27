#!/bin/bash

# SAIA 一键安装脚本 - Linux
set -e

echo ==================================================
echo [Linux] SAIA 一键安装脚本
echo ==================================================
echo

# 1. 检查是否安装了GCC
if ! command -v gcc &> /dev/null; then
    echo "[!] 未检测到 GCC 编译器"
    echo "[!] 正在自动安装 build-essential..."
    
    # 检测系统发行版并安装
    if [ -f /etc/debian_version ]; then
        sudo apt-get update && sudo apt-get install -y build-essential
    elif [ -f /etc/redhat-release ]; then
        sudo yum install -y gcc make
    elif [ -f /etc/arch-release ]; then
        sudo pacman -S --noconfirm base-devel
    else
        echo "[X] 无法自动安装编译器。请手动安装 gcc。"
        exit 1
    fi
    
    echo "[OK] GCC 安装完成！"
    echo
else
    echo "[OK] 检测到 GCC 编译器"
    echo
fi

# 2. 开始编译
echo "[!] 正在编译 SAIA..."
if [ -f Makefile ]; then
    make
else
    gcc *.c -o saia -lpthread -lm -std=c11 -Wall -O2
fi

if [ $? -ne 0 ]; then
    echo "[X] 编译失败！请检查错误信息。"
    exit 1
fi

echo
echo "[OK] 编译成功！生成文件: saia"
echo
echo ==================================================
echo "安装完成！"
echo "您可以运行: ./saia"
echo ==================================================
chmod +x saia
exit 0
