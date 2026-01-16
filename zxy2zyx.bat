@echo off
:: 切换代码页为 UTF-8，解决中文显示问题
chcp 65001 >nul
setlocal enabledelayedexpansion

:: 获取脚本所在目录作为工作目录
set "workDir=%~dp0"
cd /d "%workDir%"

echo 正在开始转换瓦片结构 [z/x/y -^> z/y/x]...
echo 工作目录: %workDir%
echo ------------------------------------------------

set "totalFileCount=0"
set "totalErrorCount=0"

:: 获取当前时间戳用于日志文件名
for /f "tokens=1-6 delims=/:. " %%a in ("%date% %time%") do (
    set "timestamp=%%a%%b%%c_%%d%%e%%f"
)
set "timestamp=%timestamp: =0%"

:: 1. 先获取所有 z 层级列表
set "zCount=0"
for /d %%z in (*) do (
    if exist "%%z\" (
        set /a zCount+=1
        set "zList[!zCount!]=%%z"
    )
)

:: 2. 逐个处理每个 z 层级（使用子程序减少内存占用）
for /l %%i in (1,1,%zCount%) do (
    call :ProcessZLevel "!zList[%%i]!" "%timestamp%"
)

echo ------------------------------------------------
echo 共转换 %totalFileCount% 个文件
if %totalErrorCount% gtr 0 (
    echo 错误: %totalErrorCount% 个文件转换失败
)

echo ------------------------------------------------
echo 正在清理空文件夹...

:: 3. 清理所有空文件夹（最多尝试3次，因为可能有嵌套）
for /l %%n in (1,1,3) do (
    for /f "delims=" %%d in ('dir /s /b /ad 2^>nul ^| sort /r') do (
        rd "%%d" 2>nul
    )
)

echo ------------------------------------------------
echo 处理完成！
pause
goto :eof

:: ====================================
:: 子程序：处理单个 z 层级
:: 参数1: z 层级名称
:: 参数2: 时间戳
:: ====================================
:ProcessZLevel
setlocal enabledelayedexpansion
set "zLevel=%~1"
set "timestamp=%~2"
set "zFileCount=0"
set "zErrorCount=0"

echo 正在处理层级: %zLevel%

:: 遍历 z/x 结构
for /d %%x in ("%zLevel%\*") do (
    call :ProcessXFolder "%zLevel%" "%%x"
)

:: 生成该层级的日志文件
set "logFile=%workDir%zxy2zyx_z%zLevel%_%timestamp%.log"
(
    echo ========================================
    echo 瓦片转换日志 - 层级 %zLevel%
    echo ========================================
    echo 转换时间: %date% %time%
    echo 工作目录: %workDir%
    echo 层级编号: %zLevel%
    echo 转换结构: z/x/y -^> z/y/x
    echo ----------------------------------------
    echo 成功转换: !zFileCount! 个文件
    echo 转换失败: !zErrorCount! 个文件
    echo ========================================
) > "!logFile!"

echo   ^> 已生成日志: zxy2zyx_z%zLevel%_%timestamp%.log
echo.

:: 更新全局计数器
endlocal & (
    set /a totalFileCount+=%zFileCount%
    set /a totalErrorCount+=%zErrorCount%
)
goto :eof

:: ====================================
:: 子程序：处理单个 x 文件夹
:: 参数1: z 层级名称
:: 参数2: x 文件夹路径
:: ====================================
:ProcessXFolder
setlocal enabledelayedexpansion
set "zName=%~1"
set "xPath=%~2"
set "xName=%~nx2"
set "batchCount=0"

:: 遍历 x 文件夹下的所有文件
for %%y in ("%xPath%\*") do (
    if not exist "%%y\" (
        set "yName=%%~ny"
        set "ext=%%~xy"

        :: 构建新路径 z/y/x.png（统一加 .png 后缀）
        set "newDir=%zName%\!yName!"
        set "newPath=!newDir!\%xName%.png"

        :: 创建目标文件夹
        if not exist "!newDir!\" mkdir "!newDir!" 2>nul

        :: 移动文件并统一添加 .png 后缀
        move /y "%%y" "!newPath!" >nul 2>&1
        if !errorlevel! equ 0 (
            set /a batchCount+=1
            set /a zFileCount+=1
        ) else (
            set /a zErrorCount+=1
        )
    )
)

:: 返回上层更新计数器
endlocal & (
    set /a zFileCount+=%batchCount%
    if %batchCount% gtr 0 if %totalFileCount% lss 10 (
        set /a totalFileCount+=%batchCount%
        echo   已处理 %xName%: %batchCount% 个文件
    )
)
goto :eof
