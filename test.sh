#!/bin/bash
# 快速测试脚本

echo "======================================"
echo "SAIA 快速测试"
echo "======================================"
echo ""

# 检查saia可执行文件
if [ ! -f "./saia" ]; then
    echo "错误: 未找到saia可执行文件"
    echo "请先运行 ./build_linux.sh 编译"
    exit 1
fi

echo "✓ 找到saia可执行文件"
echo ""

# 检查输入文件
echo "检查输入文件:"
FILES_OK=1

if [ -f "ip.txt" ]; then
    IP_COUNT=$(wc -l < ip.txt)
    echo "  ✓ ip.txt 存在 (共 $IP_COUNT 个IP)"
else
    echo "  ✗ ip.txt 不存在"
    FILES_OK=0
fi

if [ -f "pass.txt" ]; then
    PASS_COUNT=$(wc -l < pass.txt)
    echo "  ✓ pass.txt 存在 (共 $PASS_COUNT 个密码)"
else
    echo "  ✗ pass.txt 不存在"
    FILES_OK=0
fi

# 检查备用文件名
if [ ! -f "ip.txt" ] && [ -f "nodes.list" ]; then
    echo "  ✓ 找到备用文件 nodes.list"
fi

if [ ! -f "pass.txt" ] && [ -f "tokens.list" ]; then
    echo "  ✓ 找到备用文件 tokens.list"
fi

echo ""

if [ $FILES_OK -eq 0 ]; then
    echo "警告: 缺少输入文件"
    echo "请创建 ip.txt 和 pass.txt"
    echo ""
    echo "ip.txt 格式示例:"
    echo "  192.168.1.1"
    echo "  192.168.1.2:8080"
    echo "  10.0.0.0/24"
    echo ""
    echo "pass.txt 格式示例:"
    echo "  admin"
    echo "  admin:password123"
    echo ""
fi

# 显示系统信息
echo "系统信息:"
echo "  操作系统: $(uname -s)"
echo "  架构: $(uname -m)"
echo "  主机名: $(hostname)"
echo "  用户: $(whoami)"
echo "  当前目录: $(pwd)"
echo ""

# 显示内存和网络
echo "资源信息:"
if [ -f "/proc/meminfo" ]; then
    MEM_TOTAL=$(grep MemTotal /proc/meminfo | awk '{print $2}')
    MEM_AVAIL=$(grep MemAvailable /proc/meminfo | awk '{print $2}')
    echo "  总内存: $((MEM_TOTAL / 1024)) MB"
    echo "  可用内存: $((MEM_AVAIL / 1024)) MB"
fi
echo ""

# 检查权限
echo "文件权限:"
ls -lh saia ip.txt pass.txt 2>/dev/null || true
echo ""

# 提供测试选项
echo "======================================"
echo "测试选项:"
echo "======================================"
echo ""
echo "1. 运行交互式模式 (./saia)"
echo "2. 运行并立即退出 (echo 0 | ./saia)"
echo "3. 后台运行并记录日志 (nohup ./saia > saia.log 2>&1 &)"
echo "4. 查看可用的IP列表 (head -20 ip.txt)"
echo "5. 查看可用的密码列表 (cat pass.txt)"
echo ""
read -p "请选择 [1-5]: " choice

case $choice in
    1)
        echo ""
        echo "启动交互式模式..."
        echo ""
        ./saia
        ;;
    2)
        echo ""
        echo "快速运行测试..."
        echo ""
        echo "0" | ./saia
        ;;
    3)
        echo ""
        echo "后台运行..."
        nohup ./saia > saia.log 2>&1 &
        PID=$!
        echo "进程ID: $PID"
        echo "日志文件: saia.log"
        echo ""
        echo "查看日志: tail -f saia.log"
        echo "停止进程: kill $PID"
        ;;
    4)
        echo ""
        echo "IP列表 (前20行):"
        echo "---"
        head -20 ip.txt
        echo ""
        if [ $(wc -l < ip.txt) -gt 20 ]; then
            echo "... 还有 $(( $(wc -l < ip.txt) - 20 )) 行"
        fi
        ;;
    5)
        echo ""
        echo "密码列表:"
        echo "---"
        cat pass.txt
        ;;
    *)
        echo "无效选择"
        exit 1
        ;;
esac

echo ""
echo "======================================"
echo "测试完成"
echo "======================================"
