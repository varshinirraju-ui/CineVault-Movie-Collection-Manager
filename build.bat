@echo off
REM Build CineVault on Windows with GCC (MSYS2 / MinGW-w64).
REM Run this from the CineVault folder:  build.bat
cd /d "%~dp0"

where gcc >nul 2>nul
if errorlevel 1 (
    echo GCC was not found. Install MSYS2, then in the "MSYS2 UCRT64" terminal run:
    echo     pacman -S mingw-w64-ucrt-x86_64-gcc
    echo and add C:\msys64\ucrt64\bin to your PATH.
    exit /b 1
)

gcc -Wall -Wextra -O2 server.c movies.c -o cinevault.exe -lws2_32
if errorlevel 1 (
    echo Build failed. See the errors above.
    exit /b 1
)
echo Built cinevault.exe. Start it with run.bat or by typing: cinevault.exe
