@echo off
rem Copies Playground.vst3 into the standard VST3 folder. Run as Administrator
rem (right click -> "Run as administrator"), otherwise Windows blocks the copy.

set "SRC=%~dp0Playground.vst3"
set "DST=%CommonProgramFiles%\VST3"

if not exist "%SRC%" (
  echo Playground.vst3 was not found next to this file.
  pause
  exit /b 1
)

if not exist "%DST%" mkdir "%DST%"

xcopy "%SRC%" "%DST%\Playground.vst3\" /E /I /Y >nul
if errorlevel 1 (
  echo Copy failed. Please run this file as Administrator.
  pause
  exit /b 1
)

echo Installed to: %DST%\Playground.vst3
echo Now open FL Studio: Options - Manage plugins - Find installed plugins.
pause
