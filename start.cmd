@echo off
cd /d "%~dp0"
if not exist "build\AirAlertMap.exe" (
  echo Build the application first: powershell -File build.ps1
  pause
  exit /b 1
)
start "" "build\AirAlertMap.exe"
