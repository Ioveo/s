@echo off
setlocal
cd /d "%~dp0"

echo ==================================================
echo [Windows] SAIA 一键安装脚本
echo ==================================================
echo.

:: 1. 检查是否安装了GCC
where gcc >nul 2>nul
if %errorlevel% neq 0 (
    echo [!] 未检测到 GCC 编译器
    echo [!] 正在尝试使用 winget 安装 TDM-GCC...
    echo.
    
    where winget >nul 2>nul
    if %errorlevel% neq 0 (
        echo [X] 未找到 winget 包管理器。
        echo [!] 请手动下载并安装 TDM-GCC: https://github.com/jmeubank/tdm-gcc/releases/download/v10.3.0-tdm64-2/tdm64-gcc-10.3.0-2.exe
        echo.
        pause
        exit /b 1
    )
    
    winget install "TDM-GCC" --silent --accept-source-agreements --accept-package-agreements
    if %errorlevel% neq 0 (
        echo [X] 自动安装失败。请手动安装。
        pause
        exit /b 1
    )
    
    echo [OK] TDM-GCC 安装完成！请重启此脚本以生效。
    pause
    exit /b 0
)

echo [OK] 检测到 GCC 编译器
echo.

:: 2. 开始编译
echo [!] 正在编译 SAIA...
gcc *.c -o saia.exe -lws2_32 -lwinhttp -std=c11 -Wall -O2

if %errorlevel% neq 0 (
    echo [X] 编译失败！请检查错误信息。
    pause
    exit /b 1
)

echo.
echo [OK] 编译成功！生成文件: saia.exe
echo.
echo ==================================================
echo 安装完成！您可以直接运行 saia.exe
echo ==================================================
pause
