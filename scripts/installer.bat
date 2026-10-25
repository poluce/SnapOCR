@echo off
rem ===========================================================================
rem SnapOCR installer builder (Windows / Inno Setup 6)
rem
rem Usage:
rem   scripts\installer.bat           build Release package then setup.exe
rem   scripts\installer.bat --skip-package
rem
rem Requires Inno Setup 6 (ISCC.exe). CI installs it via choco; local dev can
rem install from https://jrsoftware.org/isdl.php (per-user install is fine).
rem ===========================================================================
setlocal EnableExtensions EnableDelayedExpansion
set "REPO_ROOT=%~dp0.."
pushd "%REPO_ROOT%" >nul

set "SKIP_PACKAGE=0"
if /i "%~1"=="--skip-package" set "SKIP_PACKAGE=1"

rem ---- locate ISCC ----
set "ISCC="
for %%P in (
    "%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe"
    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
    "C:\Program Files\Inno Setup 6\ISCC.exe"
) do (
    if exist %%P if not defined ISCC set "ISCC=%%~P"
)
if not defined ISCC (
    for /f "delims=" %%I in ('where ISCC.exe 2^>nul') do if not defined ISCC set "ISCC=%%I"
)
if not defined ISCC (
    echo [ERROR] ISCC.exe not found. Install Inno Setup 6 first.
    popd >nul
    exit /b 1
)

rem ---- version from CMakeLists.txt ----
set "VERSION="
for /f "tokens=1,2" %%A in ('findstr /r /c:"^    VERSION " CMakeLists.txt') do (
    if "%%A"=="VERSION" set "VERSION=%%B"
)
if not defined VERSION (
    echo [ERROR] Unable to read VERSION from CMakeLists.txt.
    popd >nul
    exit /b 1
)
set "VERSION4=%VERSION%.0"

if "%SKIP_PACKAGE%"=="0" (
    call "%~dp0package.bat" Release
    if errorlevel 1 (
        echo [ERROR] Package step failed.
        popd >nul
        exit /b 1
    )
)

if not exist "dist\SnapOCR-%VERSION%\SnapOCR.exe" (
    echo [ERROR] dist\SnapOCR-%VERSION% not found. Run scripts\package.bat Release first.
    popd >nul
    exit /b 1
)

echo [INFO] Building installer for SnapOCR %VERSION%
"%ISCC%" /Qp /DMyAppVersion=%VERSION% /DMyAppVersion4=%VERSION4% installer\snapocr.iss
if errorlevel 1 (
    echo [ERROR] ISCC failed.
    popd >nul
    exit /b 1
)

set "SETUP=dist\SnapOCR-%VERSION%-setup.exe"
if not exist "%SETUP%" (
    echo [ERROR] %SETUP% not produced.
    popd >nul
    exit /b 1
)
echo [OK] Installer ready: %SETUP%
dir /b "%SETUP%" >nul
popd >nul
exit /b 0
