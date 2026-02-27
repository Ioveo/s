#!/bin/bash
# 快速测试脚本 - 验证文件和程序状态

echo "======================================"
echo "SAIA 文件验证测试"
echo "======================================"
echo ""

# 检查必需文件
echo "【1. 检查必需文件】"
MISSING=0

FILES=(saia.h main.c config.c file_ops.c network.c scanner.c json_parser.c utils.c color.c string_ops.c backpressure.c Makefile build_linux.sh test.sh ip.txt pass.txt)

for file in "${FILES[@]}"; do
    if [ -f "$file" ]; then
        echo "  ✓ $file"
    else
        echo "  ✗ $file (缺失)"
        MISSING=$((MISSING+1))
    fi
done

echo ""
if [ $MISSING -gt 0 ]; then
    echo "❌ 缺少 $MISSING 个文件，请先上传"
    exit 1
else
    echo "✅ 所有必需文件都存在"
fi

# 检查文件权限
echo ""
echo "【2. 检查文件权限】"
if [ -x "build_linux.sh" ]; then
    echo "  ✓ build_linux.sh 可执行"
else
    echo "  ⚠ build_linux.sh 不可执行, 正在添加..."
    chmod +x build_linux.sh
fi

if [ -x "test.sh" ]; then
    echo "  ✓ test.sh 可执行"
else
    echo "  ⚠ test.sh 不可执行, 正在添加..."
    chmod +x test.sh
fi

# 检查IP文件格式
echo ""
echo "【3. 验证IP文件】"
if [ -f "ip.txt" ]; then
    IP_COUNT=$(wc -l < ip.txt)
    echo "  ✓ ip.txt 存在"
    echo "    - IP数量: $IP_COUNT"
    echo "    - 示例IP:"
    head -3 ip.txt | sed 's/^/      /'
    
    # 验证IP格式
    INVALID=0
    while IFS= read -r line; do
        # 移除行号和冒号
        clean_ip=$(echo "$line" | sed 's/^[0-9]*:\s*//' | head -1)
        if [[ ! "$clean_ip" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
            INVALID=$((INVALID+1))
        fi
    done < ip.txt
    
    if [ $INVALID -eq 0 ]; then
        echo "    ✓ 所有IP格式正确"
    else
        echo "    ⚠ $INVALID 个IP格式可能有问题"
    fi
else
    echo "  ✗ ip.txt 不存在"
fi

# 检查密码文件格式
echo ""
echo "【4. 验证密码文件】"
if [ -f "pass.txt" ]; then
    PASS_COUNT=$(wc -l < pass.txt)
    echo "  ✓ pass.txt 存在"
    echo "    - 密码组合数: $PASS_COUNT"
    echo "    - 示例密码:"
    head -3 pass.txt | sed 's/^/      /'
    
    # 格式检查
    VALID_COLONS=$(grep -c ":" pass.txt | head -1)
    echo "    - 包含冒号的行: $VALID_COLONS"
else
    echo "  ✗ pass.txt 不存在"
fi

# 检查gcc
echo ""
echo "【5. 检查编译环境】"
if command -v gcc &> /dev/null; then
    GCC_VERSION=$(gcc --version | head -1)
    echo "  ✓ GCC 已安装"
    echo "    版本: $GCC_VERSION"
else
    echo "  ✗ GCC 未安装"
    echo "    请运行: sudo apt-get install build-essential"
    exit 1
fi

# 检查是否已编译
echo ""
echo "【6. 检查编译状态】"
if [ -f "./saia" ]; then
    echo "  ✓ saia 可执行文件已存在"
    ls -lh saia | awk '{print "    大小: " $5 ", 修改时间: " $6 " " $7 " " $8}'
    
    # 测试是否可执行
    if [ -x "./saia" ]; then
        echo "    ✓ 文件有执行权限"
    else
        echo "    ⚠ 文件无执行权限, 正在添加..."
        chmod +x ./saia
    fi
else
    echo "  ⚠ saia 可执行文件不存在"
    echo "    需要编译: ./build_linux.sh"
fi

echo ""
echo "======================================"
echo "测试总结"
echo "======================================"
echo ""

# 计算理论扫描量
if [ -f "ip.txt" ] && [ -f "pass.txt" ]; then
    TOTAL=$(( $IP_COUNT * $PASS_COUNT ))
    echo "📊 扫描规模："
    echo "   IP节点: $IP_COUNT"
    echo "   密码组合: $PASS_COUNT"
    echo "   理论尝试次数: $TOTAL"
    echo ""
    
    # 估算时间
    echo "⏱️  时间估算: "
    echo "   假设每个尝试3秒:"
    SECONDS_TOTAL=$(( TOTAL * 3 ))
    if [ $SECONDS_TOTAL -gt 3600 ]; then
        echo "   约 $(( SECONDS_TOTAL / 3600 )) 小时"
    elif [ $SECONDS_TOTAL -gt 60 ]; then
        echo "   约 $(( SECONDS_TOTAL / 60 )) 分钟"
    else
        echo "   约 $SECONDS_TOTAL 秒"
    fi
fi

# 检查是否一切就绪
if [ -f "./saia" ] && [ -f "ip.txt" ] && [ -f "pass.txt" ]; then
    echo ""
    echo "✅ 系统就绪！可以运行测试"
    echo ""
    echo "运行命令："
    echo "  ./test.sh        # 交互式测试"
    echo "  ./saia           # 直接运行程序"
    echo "  nohup ./saia > saia.log 2>&1 &  # 后台运行"
else
    echo ""
    echo "⚠️  需要先编译程序"
    echo ""
    echo "编译命令: ./build_linux.sh"
fi

echo ""
echo "======================================"
