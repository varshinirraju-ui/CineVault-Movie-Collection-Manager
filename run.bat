@echo off
REM Build (if needed), start the CineVault server and open it in the browser.
cd /d "%~dp0"

if not exist cinevault.exe (
    call build.bat
    if errorlevel 1 exit /b 1
)

start "" http://localhost:8080
cinevault.exe
