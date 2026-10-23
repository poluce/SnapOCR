@echo off
rem Launch the local Debug/Release build with the Qt runtime on PATH.
rem Usage: scripts\run.bat [SnapOCR arguments...]
setlocal EnableExtensions

set "REPO_ROOT=%~dp0.."
if not defined QTDIR set "QTDIR=E:\Qt6\6.11.1\msvc2022_64"

if not exist "%REPO_ROOT%\build\SnapOCR.exe" (
    echo [ERROR] build\SnapOCR.exe not found. Run scripts\build.bat first.
    exit /b 1
)

set "PATH=%QTDIR%\bin;%PATH%"
start "" "%REPO_ROOT%\build\SnapOCR.exe" %*
exit /b 0
