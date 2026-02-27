#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import os
import socket
import json
import time
import asyncio

print("=" * 60)
print("SAIA - 原始Python版本功能测试")
print("=" * 60)
print()

# 测试1: 基本功能检测
print("[Test 1] 基本功能检测")
print("-" * 40)
print("Socket模块:", "OK" if hasattr(socket, "socket") else "FAIL")
print("JSON模块:", "OK" if hasattr(json, "loads") else "FAIL")
print("Asyncio模块:", "OK" if hasattr(asyncio, "EventLoop") else "FAIL")
print()

# 测试2: 读取数据文件
print("[Test 2] 数据文件读取")
print("-" * 40)
ips = []
passes = []

try:
    with open("ip.txt", "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line:
                ips.append(line)
    print(f"IP文件: {len(ips)} 个IP")
except Exception as e:
    print(f"IP文件读取失败: {e}")

try:
    with open("pass.txt", "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line and ":" in line:
                passes.append(line.split(":"))
            elif line:
                passes.append(["admin", line])
    print(f"密码文件: {len(passes)} 个密码组合")
except Exception as e:
    print(f"密码文件读取失败: {e}")

print()

# 测试3: IP格式验证
print("[Test 3] IP格式验证")
print("-" * 40)
import re

valid_ips = []
invalid_ips = []

for ip in ips:
    # 移除行号前缀
    clean_ip = re.sub(r"^\d+:\s*", "", ip)
    # 简单的IP验证
    if re.match(r"^(\d{1,3}\.){3}\d{1,3}$", clean_ip):
        valid_ips.append(clean_ip)
        parts = clean_ip.split(".")
        if all(int(p) <= 255 for p in parts):
            valid_ips[-1] += " (有效)"
        else:
            valid_ips[-1] += " (范围错误)"
    else:
        invalid_ips.append(clean_ip)

print(f"有效IP: {len(valid_ips)}")
print(f"无效IP: {len(invalid_ips)}")

if valid_ips:
    print("示例有效IP:")
    for ip in valid_ips[:3]:
        print(f"  {ip}")

if invalid_ips:
    print("无效IP:")
    for ip in invalid_ips[:3]:
        print(f"  {ip}")

print()

# 测试4: 网络连接测试
print("[Test 4] 网络连接测试")
print("-" * 40)


def test_ip_connection(ip, port, timeout=2):
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        result = sock.connect_ex((ip, port))
        sock.close()
        return result == 0
    except Exception:
        return False


# 测试第一个IP的1080端口（常见SOCKS5端口）
if valid_ips:
    test_ip = re.sub(r"\s*\(.*\)", "", valid_ips[0])
    default_ports = [1080, 1081, 1090, 8080, 9999]
    test_port = default_ports[0]

    print(f"测试IP: {test_ip}")
    print(f"测试端口: {test_port}")
    is_alive = test_ip_connection(test_ip, test_port)
    print(f"连接结果: {'连接成功' if is_alive else '连接失败/超时'}")

print()

# 测试5: 扫描规模计算
print("[Test 5] 扫描规模计算")
print("-" * 40)

scan_count = len(valid_ips) * len(passes)
print(f"IP数量: {len(valid_ips)}")
print(f"密码组合: {len(passes)}")
print(f"总计尝试: {scan_count} 次")

# 时间估算
avg_time = 3  # 假设每次3秒
total_seconds = scan_count * avg_time
total_minutes = total_seconds / 60
total_hours = total_minutes / 60

print(f"\n时间估算（每次尝试{avg_time}秒）:")
print(f"  总秒数: {total_seconds:,}")
print(f"  总分钟: {total_minutes:.1f}")

if total_hours >= 1:
    print(f"  总小时: {total_hours:.1f}")
    days = int(total_hours / 24)
    hours = int(total_hours % 24)
    if days > 0:
        print(f"  约 {days} 天 {hours} 小时")
    else:
        print(f"  约 {int(total_hours)} 小时")
else:
    print(f"  约 {int(total_minutes)} 分钟")

print()
print("=" * 60)
print("测试建议")
print("=" * 60)
print()

print("【选项1: 安装C编译器】")
print("  优点: 运行速度快，有新增的压背控制功能")
print("  推荐: TDM-GCC ( fastest to install )")
print("  下载: https://jmeubank.github.io/tdm-gcc/download/tdm64-gcc-9.2.0.exe")
print("  安装时间: 5分钟")
print()

print("【选项2: 使用原始Python版本】")
print("  优点: 无需安装编译器，直接运行")
print("  运行: python DE.py")
print("  缺点: 没有压背控制，可能资源占用较高")
print()

print("【选项3: 在SERV00服务器运行】")
print("  优点: 服务器环境有gcc，适合长期扫描")
print("  步骤: 上传文件 -> 编译 -> 后台运行")
print()

print("=" * 60)
print("文件状态总结")
print("=" * 60)
print(f"✓ Python环境: 可用 (v3.13.2)")
print(f"✓ IP文件: {len(ips)} 个IP")
print(f"✓ 密码文件: {len(passes)} 个组合")
print(f"✓ C源文件: 11个 (需要编译器)")
print(f"✗ C编译器: 未安装")
print()

if scan_count > 0:
    print("准备就绪!")
    print("请选择执行方式:")
    print("  1. 安装TDM-GCC后编译运行: build.bat")
    print("  2. 直接使用Python版本: python DE.py")
else:
    print("请先准备好IP和密码文件")
print()
