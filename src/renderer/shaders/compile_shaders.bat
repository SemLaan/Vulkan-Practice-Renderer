@echo off
rem for /R %~DP0 %%i in (*.frag, *.vert) do echo shaders/%%~ni%%~xi.spv
rem echo %~DP0
xcopy /f /y %~DP0*.frag shaders/
xcopy /f /y %~DP0*.vert shaders/
for /R %~DP0 %%i in (*.frag, *.vert) do %VULKAN_SDK%/Bin/glslc %%i -I %~DP0 -o shaders/%%~ni%%~xi.spv
echo done