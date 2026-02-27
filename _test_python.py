#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import os

print("=" * 50)
print("Python 环境检测")
print("=" * 50)
print()

print("Python 版本:", sys.version)
print("Python 路径:", sys.executable)
print()

print("模块检查:")
modules = [
    "asyncio",
    "json",
    "socket",
    "time",
    "os",
    "re",
    "threading",
    "signal",
    "subprocess",
]
for m in modules:
    try:
        __import__(m)
        print(f"  OK: {m}")
    except ImportError as e:
        print(f"  FAIL: {m} - {e}")

print()
print("=" * 50)
print("数据文件检查")
print("=" * 50)
print()

# 检查 ip.txt
if os.path.exists("ip.txt"):
    with open("ip.txt", "r", encoding="utf-8") as f:
        ips = f.readlines()
    print(f"ip.txt: {len(ips)} 行")
    print("前3个IP:")
    for ip in ips[:3]:
        print(f"  {ip.strip()}")
else:
    print("ip.txt: 不存在")

print()

# 检查 pass.txt
if os.path.exists("pass.txt"):
    with open("pass.txt", "r", encoding="utf-8") as f:
        passes = f.readlines()
    print(f"pass.txt: {len(passes)} 行")
    print("前3个密码:")
    for p in passes[:3]:
        print(f"  {p.strip()}")
else:
    print("pass.txt: 不存在")

print()
print("=" * 50)
print("C 编译器检查")
print("=" * 50)
print()

import shutil

# 检查 GCC
gcc_path = shutil.which("gcc")
if gcc_path:
    print(f"GCC 找到: {gcc_path}")
    import subprocess

    try:
        result = subprocess.run([gcc_path, "--version"], capture_output=True, text=True)
        print(f"GCC 版本: {result.stdout.split(chr(10))[0]}")
    except:
        pass
else:
    print("GCC: 未找到")
    print("  请安装:")
    print("  - TDM-GCC: https://jmeubank.github.io/tdm-gcc/")
    print("  - MinGW-w64: https://www.mingw-w64.org/")
    print("  - 或者使用MSYS2: https://www.msys2.org/")

# 检查 cl

cl_path = shutil.which("cl")
if cl_path:
    print(f"MSVC cl 找到: {cl_path}")
else:
    print("MSVC cl: 未找到")

print()
print("=" * 50)
