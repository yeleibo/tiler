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

set "fileCount=0"
set "errorCount=0"

:: 1. 遍历所有 z 文件夹下的 png 文件
for /d %%z in (*) do (
    if exist "%%z\" (
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
                        set /a fileCount+=1
                        if !fileCount! leq 10 (
                            echo   [!fileCount!] %%z/%%~nx/%%~ny!ext! -^> !newPath!
                        )
                    ) else (
                        set /a errorCount+=1
                        echo   [错误] 无法移动: %%y
                    )
                )
            )
        )
    )
)

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