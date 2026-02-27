快速部署脚本 - 一键上传并测试

# 使用方法

1. 将此文件放置在 F:\C\ 目录
2. 修改下面的服务器信息
3. 运行此脚本

```powershell
# Windows PowerShell 部署脚本

# ==================== 配置区域 ====================

# 你的SERV00服务器信息
$SERVER_USER = "your_username"        # 替换为你的用户名
$SERVER_HOST = "your-server.serv00.com"  # 替换为你的服务器地址
$LOCAL_DIR = "F:\C"
$REMOTE_DIR = "/home/$SERVER_USER/saia"

# 文件列表
$FILES = @(
    "saia.h",
    "main.c", "config.c", "file_ops.c", "network.c",
    "scanner.c", "json_parser.c", "utils.c", "color.c",
    "string_ops.c", "backpressure.c",
    "Makefile", "build_linux.sh", "test.sh"
)

# ==================== 脚本开始 ====================

Write-Host "======================================" -ForegroundColor Cyan
Write-Host "SAIA 自动部署脚本" -ForegroundColor Cyan
Write-Host "======================================" -ForegroundColor Cyan
Write-Host ""

# 检查文件是否存在
Write-Host "检查本地文件..." -ForegroundColor Yellow
foreach ($file in $FILES) {
    $filepath = Join-Path $LOCAL_DIR $file
    if (Test-Path $filepath) {
        Write-Host "  ✓ $file" -ForegroundColor Green
    } else {
        Write-Host "  ✗ $file (不存在)" -ForegroundColor Red
    }
}

# 询问是否继续
Write-Host ""
$continue = Read-Host "是否继续上传到 $SERVER_USER@$SERVER_HOST ? [Y/n]"
if ($continue -ne "" -and $continue -ne "Y" -and $continue -ne "y") {
    exit
}

# 检查scp命令
Write-Host ""
Write-Host "检查SCP..." -ForegroundColor Yellow
$scp = Get-Command scp -ErrorAction SilentlyContinue
if (-not $scp) {
    Write-Host "错误: 未找到scp命令" -ForegroundColor Red
    Write-Host ""
    Write-Host "请安装以下之一:" -ForegroundColor Yellow
    Write-Host "  1. WinSCP: https://winscp.net/"
    Write-Host "  2. Git Bash (包含scp)"
    Write-Host "  3. Windows Subsystem Linux (WSL)"
    exit 1
}
Write-Host "  ✓ 找到: $($scp.Source)" -ForegroundColor Green

# 上传文件
Write-Host ""
Write-Host "开始上传文件..." -ForegroundColor Yellow
$all_files = $FILES | ForEach-Object { Join-Path $LOCAL_DIR $_ }

# 使用scp上传
scp $all_files "$SERVER_USER@$SERVER_HOST`:~"

if ($LASTEXITCODE -eq 0) {
    Write-Host "  ✓ 上传成功" -ForegroundColor Green
} else {
    Write-Host "  ✗ 上传失败" -ForegroundColor Red
    exit 1
}

# 在服务器上执行命令
Write-Host ""
Write-Host "在服务器上编译和测试..." -ForegroundColor Yellow

$commands = @"
cd ~
mkdir -p $REMOTE_DIR
mv *.h *.c *.sh Makefile $REMOTE_DIR/ 2>/dev/null
chmod +x $REMOTE_DIR/*.sh
cd $REMOTE_DIR
./build_linux.sh
"@

ssh "$SERVER_USER@$SERVER_HOST" $commands

# 下载测试
Write-Host ""
Write-Host "======================================" -ForegroundColor Cyan
Write-Host "上传完成!" -ForegroundColor Green
Write-Host "======================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "下一步操作:" -ForegroundColor Yellow
Write-Host "1. 登录到服务器:" -ForegroundColor White
Write-Host "   ssh $SERVER_USER@$SERVER_HOST" -ForegroundColor Cyan
Write-Host ""
Write-Host "2. 进入目录:" -ForegroundColor White
Write-Host "   cd $REMOTE_DIR" -ForegroundColor Cyan
Write-Host ""
Write-Host "3. 运行测试:" -ForegroundColor White
Write-Host "   ./test.sh" -ForegroundColor Cyan
Write-Host ""
Write-Host "4. 运行程序:" -ForegroundColor White
Write-Host "   ./saia" -ForegroundColor Cyan
Write-Host ""
