@echo off
REM SAIA C语言版本编译脚本 (Windows)

echo ========================================
echo SAIA 编译脚本 - Windows版本
echo ========================================
echo.

REM 检查编译器
WHERE gcc >nul 2>&1
IF %ERRORLEVEL% EQU 0 (
    echo 找到 GCC 编译器
    set COMPILER=gcc
    set LINK_OPTS=-lws2_32 -lwinhttp
    goto :compile
)

WHERE mingw32-gcc >nul 2>&1
IF %ERRORLEVEL% EQU 0 (
    echo 找到 MinGW32 GCC 编译器
    set COMPILER=mingw32-gcc
    set LINK_OPTS=-lws2_32 -lwinhttp
    goto :compile
)

WHERE cl >nul 2>&1
IF %ERRORLEVEL% EQU 0 (
    echo 找到 Visual Studio 编译器
    echo 注意: 需要先运行 vcvarsall.bat
    set COMPILER=cl
    set LINK_OPTS=/link ws2_32.lib winhttp.lib
    goto :compile_vs
)

echo 错误: 未找到C编译器!
echo.
echo 请安装以下编译器之一:
echo   1. MinGW-w64 (推荐): https://www.mingw-w64.org/
echo   2. MSYS2: https://www.msys2.org/
echo   3. Visual Studio: https://visualstudio.microsoft.com/
echo.
pause
exit /b 1

:compile
echo.
echo 正在编译...
echo.

set FLAGS=-std=c11 -Wall -O2 -D_GNU_SOURCE

%COMPILER% %FLAGS% ^
    main.c missing_functions.c config.c file_ops.c network.c http.c ^
    scanner.c json_parser.c utils.c ^
    color.c string_ops.c backpressure.c ^
    -o saia.exe %LINK_OPTS%

IF %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo 编译成功!
    echo ========================================
    echo.
    echo 生成文件: saia.exe
    echo.
    echo 运行命令: saia.exe
    echo.
) ELSE (
    echo.
    echo 编译失败，请检查错误信息
    echo.
)

pause
exit /b %ERRORLEVEL%

:compile_vs
echo.
echo Visual Studio 编译模式
echo.

set FLAGS=/O2 /D_CRT_SECURE_NO_WARNINGS

cl %FLAGS% ^
    main.c missing_functions.c config.c file_ops.c network.c http.c ^
    scanner.c json_parser.c utils.c ^
    color.c string_ops.c backpressure.c ^
    /Fe:saia.exe %LINK_OPTS%

IF %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo 编译成功!
    echo ========================================
    echo.
    echo 生成文件: saia.exe
    echo.
    echo 运行命令: .\saia.exe
    echo.
) ELSE (
    echo.
    echo 编译失败，请检查错误信息
    echo.
)

pause
exit /b %ERRORLEVEL%
