@echo off
setlocal
cd /d "%~dp0"
if not exist "%~dp0AirAlertMap.exe" (
  echo AirAlertMap.exe is missing. Extract the entire ZIP before starting.
  pause
  exit /b 1
)
if not exist "%~dp0check-runtime.ps1" (
  echo check-runtime.ps1 is missing. Extract the entire ZIP again.
  pause
  exit /b 1
)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0check-runtime.ps1"
if errorlevel 1 (
  pause
  exit /b 1
)
start "" "%~dp0AirAlertMap.exe"
