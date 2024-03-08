@echo off
for /R %~DP0 %%i in (*.frag, *.vert) do %VULKAN_SDK%/Bin/glslc %%i -o %%~ni.spv
echo done