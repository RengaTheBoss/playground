@echo off
rem Local Windows build. Needs: Visual Studio 2022 (or Build Tools) with "Desktop development with C++",
rem CMake and Git in PATH. The first run downloads JUCE (a few minutes).

cmake -S . -B build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 goto :fail

cmake --build build --config Release --target Playground_VST3 --parallel
if errorlevel 1 goto :fail

echo.
echo Done: build\Playground_artefacts\Release\VST3\Playground.vst3
echo Copy that folder to C:\Program Files\Common Files\VST3\ (or run install_vst3_windows.bat from there).
pause
exit /b 0

:fail
echo Build failed.
pause
exit /b 1
