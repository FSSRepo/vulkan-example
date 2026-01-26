@echo off
set GLSLC=glslc.exe
set SHADER_DIR=%~dp0shaders
set OUT_DIR=%~dp0build\Release

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

for %%f in ("%SHADER_DIR%\*") do (
    echo Compilando %%~nxf ...
    "%GLSLC%" "%%f" -o "%OUT_DIR%\%%~nxf.spv"
)
echo Shaders compilados en %OUT_DIR%
cmake --build build --config Release