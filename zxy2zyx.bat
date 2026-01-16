@echo off
:: 切换代码页为 UTF-8，解决中文显示问题
chcp 65001 >nul
setlocal enabledelayedexpansion
echo 正在开始转换瓦片结构 [z/x/y -> z/y/x]...
echo ------------------------------------------------

:: 1. 遍历所有文件 (png 和无后缀)
for /f "delims=" %%i in ('dir /s /b /a-d') do (
    set "oldPath=%%i"
    set "ext=%%~xi"

    :: 只处理 .png 文件或无后缀文件
    if "!ext!"==".png" (
        set "processFile=1"
    ) else if "!ext!"=="" (
        set "processFile=1"
    ) else (
        set "processFile=0"
    )

    if "!processFile!"=="1" (
    :: 获取文件名 (y)
    set "yName=%%~ni"

    :: 获取父目录 (x)
    for %%a in ("%%~dpi.") do set "xName=%%~na"

    :: 获取爷爷目录 (z)
    for %%b in ("%%~dpi..") do set "zName=%%~nb"

    :: 获取根目录 (z 的上一级)
    for %%c in ("%%~dpi..\..") do set "baseDir=%%~fnc"

    :: 构建新的目标文件夹和完整路径 (统一加 .png 后缀)
    set "newDir=!baseDir!\!zName!\!yName!"
    set "newPath=!newDir!\!xName!.png"

    :: 如果新文件夹不存在则创建
    if not exist "!newDir!" mkdir "!newDir!"

    :: 移动文件并输出完整路径
    move /y "!oldPath!" "!newPath!" >nul
    echo 转换: "!oldPath!" --^> "!newPath!"
    )
)

echo ------------------------------------------------
echo 正在清理原始 x 文件夹...

:: 2. 倒序遍历所有目录并删除空文件夹 (由此彻底移除原 x 文件夹)
for /f "delims=" %%d in ('dir /s /b /ad ^| sort /r') do (
    rd "%%d" 2>nul
)

echo ------------------------------------------------
echo 处理完成！
pause