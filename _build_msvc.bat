@echo off
REM C语言版本测试脚本 - Visual Studio专用

echo ========================================
echo SAIA C语言版本编译测试
echo ========================================
echo.

REM 查找Visual Studio安装
set "VS_FOUND=0"

REM 尝试常用的VS路径
set "VS_PATHS=Program Files\Microsoft Visual Studio\2022\Community  Program Files\Microsoft Visual Studio\2022\Professional  Program Files\Microsoft Visual Studio\2022\Enterprise  Program Files (x86)\Microsoft Visual Studio\2019\Community  Program Files (x86)\Microsoft Visual Studio\2019\Professional"

for %%P in (%VS_PATHS%) do (
    if exist "C:\%%P\VC\Tools\MSVC\*" (
        echo [OK] 找到Visual Studio: C:\%%P
        set "VS_FOUND=1"
        set "VS_INST_PATH=C:\%%P"
        goto :found_vs
    )
)

if %VS_FOUND%==0 (
    echo [!] 未在默认路径找到Visual Studio
    echo.
    echo 请确认:
    echo   1. Visual Studio是否完全安装?
    echo   2. 是否选择了"使用C++的桌面开发"?
    echo   3. 是否安装了MSVC编译器?
    echo.
    echo 建议:
    echo   - 打开Visual Studio Installer
    echo   - 修改 -^> 更多 -^> 使用C++的桌面开发
    echo   - 确保勾选: MSVC v143 - VS 2022 C++ x64/x86 生成工具
    echo.
    pause
    exit /b 1
)

:found_vs
echo.
echo ========================================
echo 查找编译器...
echo ========================================
echo.

REM 查找cl.exe
set "CL_FOUND=0"

for /f "delims=" %%F in ('dir /s /b "%VS_INST_PATH%\VC\Tools\MSVC\*\bin\Hostx64\x64\cl.exe" 2^>nul') do (
    echo [OK] 找到编译器: %%F
    set "CL_FULL_PATH=%%F"
    set "CL_FOUND=1"
    goto :found_cl
)

if %CL_FOUND%==0 (
    echo [!] 未找到cl.exe编译器
    echo.
    echo 请确保Visual Studio正确安装了C++工具
    pause
    exit /b 1
)

:found_cl
echo.
echo ========================================
echo 编译程序...
echo ========================================
echo.

REM 设置Visual Studio环境变量
for /f "delims=" %%D in ('dir /ad /b "%VS_INST_PATH%\VC\Auxiliary\Build" 2^>nul') do (
    for /f "delims=" %%V in ('dir /ad /b "%VS_INST_PATH%\VC\Auxiliary\Build\%%D" 2^>nul') do (
        if exist "%VS_INST_PATH%\VC\Auxiliary\Build\%%D\%%V\vcvars64.bat" (
            echo [INFO] 设置环境: %%V
            call "%VS_INST_PATH%\VC\Auxiliary\Build\%%D\%%V\vcvars64.bat" >nul 2>&1
            goto :vs_vars_set
        )
    )
)

:vs_vars_set

REM 编译C程序
echo [编译] 正在编译 SAIA...
echo.

cl /O2 /D_CRT_SECURE_NO_WARNINGS ^
   main.c missing_functions.c config.c file_ops.c network.c http.c scanner.c ^
   json_parser.c utils.c color.c string_ops.c backpressure.c ^
   /Fe:saia.exe

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [!] 编译失败!
    echo 请检查编译错误信息
    echo.
    pause
    exit /b 1
)

echo.
echo ========================================
echo [成功] 编译完成!
echo ========================================
echo.

if exist "saia.exe" (
    echo [OK] 生成文件: saia.exe
    dir "saia.exe" | find "saia.exe"
    echo.
    echo ========================================
    echo 文件统计:
    ========================================
    echo.
    dir /b *.c | find /c ".c"
    echo 个C源文件
    
    dir /b *.exe 2>nul | find /c ".exe"
    echo 个可执行文件
    
    echo.
    echo ========================================
    echo 测试运行:
    echo ========================================
    echo.
    echo 运行命令:
    echo   .\saia.exe
    echo.
    echo 或双击: saia.exe
    echo.
    
    echo 可选: 快速测试
    echo   echo 0 ^| .\saia.exe
    echo.
    
    choice /C YN /M "是否现在运行程序 [Y/N]?"
    if errorlevel 2 goto :end_test
    if errorlevel 1 goto :end_test
    
    echo.
    echo [运行] 启动SAIA...
    echo.
    saia.exe
    
    :end_test
)

else
    echo [!] 未找到生成的可执行文件
)

echo.
echo ========================================
pause
