@echo off
for /R %~DP0 %%i in (*.frag, *.vert) do echo shaders/%%~ni%%~xi.spv
echo %~DP0
xcopy /f /y %~DP0*.frag shaders/
xcopy /f /y %~DP0*.vert shaders/
for /R %~DP0 %%i in (*.frag, *.vert) do %VULKAN_SDK%/Bin/glslc %%i -o shaders/%%~ni%%~xi.spv
echo done