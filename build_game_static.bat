@echo off

setlocal enabledelayedexpansion enableextensions
set FileLIST=
for /R %~DP0/src %%f in (*.c) do set FileLIST=!FileLIST! %%f

set defines=-D__win__ -DDEBUG
set includepaths=-I%~DP0/src/ -I%VULKAN_SDK%/Include
set linkpaths=-L%VULKAN_SDK%/Lib
set links=-lvulkan-1
set compilerflags=-Wall -std=c17 -Wno-unused-function -g -march=native -msse3
rem -g is a debbuging flag

if not exist "%~DP0/bin/Debug" mkdir "%~DP0/bin/Debug"

echo compiling shaders...
call %~DP0/src/renderer/shaders/compile_shaders.bat

rem profiling: -pg
rem optimization: -O2

echo compiling game and engine...
gcc %FileLIST% %compilerflags% -o %~DP0/bin/Debug/Game.exe %defines% %includepaths% %linkpaths% %links%
