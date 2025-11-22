@echo off
REM Build script for MapTiler with proper MSVC environment setup

echo Setting up MSVC environment...
call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" x64

echo Building MapTiler...
cmake --build "E:\tiler\qt\cmake-build-debug-visual-studio" --target MapTiler -j 22

echo.
echo Build complete! Check output above for errors.
pause
