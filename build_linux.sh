#!/bin/bash
# SAIA编译和部署脚本 - Linux版本

echo "======================================"
echo "SAIA C版本 - 编译脚本"
echo "======================================"
echo ""

# 检查GCC编译器
if ! command -v gcc &> /dev/null; then
    echo "错误: 未找到gcc编译器"
    echo "正在尝试安装gcc..."
    
    if command -v apt-get &> /dev/null; then
        sudo apt-get update
        sudo apt-get install -y build-essential
    elif command -v yum &> /dev/null; then
        sudo yum groupinstall -y "Development Tools"
    else
        echo "无法自动安装，请手动安装gcc"
        exit 1
    fi
fi

echo "GCC版本:"
gcc --version | head -1
echo ""

# 编译选项
CFLAGS="-std=c11 -Wall -Wextra -O2 -pthread -lm"
SOURCES="main.c missing_functions.c config.c file_ops.c network.c http.c scanner.c json_parser.c utils.c color.c string_ops.c backpressure.c"
OUTPUT="saia"

echo "正在编译..."
gcc $CFLAGS $SOURCES -o $OUTPUT 2>&1 | tee compile.log

if [ $? -eq 0 ]; then
    echo ""
    echo "======================================"
    echo "✅ 编译成功!"
    echo "======================================"
    echo ""
    echo "生成文件: $OUTPUT"
    ls -lh $OUTPUT
    echo ""
    
    # 检查依赖文件
    echo "检查必需文件:"
    for file in ip.txt pass.txt; do
        if [ -f "$file" ]; then
            echo "  ✓ $file 存在"
        else
            echo "  ✗ $file 不存在 (警告)"
        fi
    done
    echo ""
    
    echo "运行命令: ./$OUTPUT"
    echo "调试运行: ./$OUTPUT --verbose"
else
    echo ""
    echo "======================================"
    echo "❌ 编译失败!"
    echo "======================================"
    echo ""
    echo "请查看错误信息:"
    cat compile.log
    exit 1
fi
