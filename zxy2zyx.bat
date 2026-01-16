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
set "timestamp=%date:~0,4%%date:~5,2%%date:~8,2%_%time:~0,2%%time:~3,2%%time:~6,2%"
set "timestamp=%timestamp: =0%"

:: 1. 遍历所有 z 文件夹下的 png 文件
for /d %%z in (*) do (
    if exist "%%z\" (
        set "zLevel=%%z"
        set "zFileCount=0"
        set "zErrorCount=0"

        echo 正在处理层级: %%z

        :: 遍历 z/x 结构
        for /d %%x in ("%%z\*") do (
            :: 遍历 z/x 目录下的所有文件（包括有后缀和无后缀）
            for %%y in ("%%x\*") do (
                if not exist "%%y\" (
                    set "zName=%%z"
                    set "xName=%%~nx"
                    set "yName=%%~ny"
                    set "ext=%%~xy"

                    :: 构建新路径 z/y/x.png（统一加 .png 后缀）
                    set "newDir=!zName!\!yName!"
                    set "newPath=!newDir!\!xName!.png"

                    :: 创建目标文件夹
                    if not exist "!newDir!\" mkdir "!newDir!"

                    :: 移动文件并统一添加 .png 后缀
                    move /y "%%y" "!newPath!" >nul 2>&1
                    if !errorlevel! equ 0 (
                        set /a zFileCount+=1
                        set /a totalFileCount+=1
                        if !totalFileCount! leq 10 (
                            echo   [!totalFileCount!] %%z/%%~nx/%%~ny!ext! -^> !newPath!
                        )
                    ) else (
                        set /a zErrorCount+=1
                        set /a totalErrorCount+=1
                        echo   [错误] 无法移动: %%y
                    )
                )
            )
        )

        :: 生成该层级的日志文件
        set "logFile=%workDir%zxy2zyx_z!zLevel!_%timestamp%.log"
        (
            echo ========================================
            echo 瓦片转换日志 - 层级 !zLevel!
            echo ========================================
            echo 转换时间: %date% %time%
            echo 工作目录: %workDir%
            echo 层级编号: !zLevel!
            echo 转换结构: z/x/y -^> z/y/x
            echo ----------------------------------------
            echo 成功转换: !zFileCount! 个文件
            echo 转换失败: !zErrorCount! 个文件
            echo ========================================
        ) > "!logFile!"

        echo   ^> 已生成日志: zxy2zyx_z!zLevel!_%timestamp%.log
        echo.
    )
)

set "fileCount=!totalFileCount!"
set "errorCount=!totalErrorCount!"

echo ------------------------------------------------
if !fileCount! gtr 10 (
    echo 共转换 !fileCount! 个文件 ^(仅显示前10个^)
) else (
    echo 共转换 !fileCount! 个文件
)
if !errorCount! gtr 0 (
    echo 错误: !errorCount! 个文件转换失败
)

echo ------------------------------------------------
echo 正在清理空文件夹...

:: 2. 清理所有空文件夹（最多尝试3次，因为可能有嵌套）
for /l %%n in (1,1,3) do (
    for /f "delims=" %%d in ('dir /s /b /ad ^| sort /r') do (
        rd "%%d" 2>nul
    )
)

echo ------------------------------------------------
echo 处理完成！
pause