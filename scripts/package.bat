@echo off
rem ===========================================================================
rem SnapOCR package script (Windows / MSVC / Qt6)
rem
rem Builds a portable folder under dist\SnapOCR-<version>\ using windeployqt,
rem then zips it to dist\SnapOCR-<version>-win64.zip.
rem
rem Usage:
rem   scripts\package.bat [Release|Debug]
rem
rem Environment:
rem   QTDIR  Qt msvc2022_64 directory, default E:\Qt6\6.11.1\msvc2022_64
rem ===========================================================================
setlocal EnableExtensions EnableDelayedExpansion

set "REPO_ROOT=%~dp0.."
pushd "%REPO_ROOT%" >nul

if not defined QTDIR set "QTDIR=E:\Qt6\6.11.1\msvc2022_64"
rem CI: Qt path is injected via PATH by install-qt-action; derive QTDIR from windeployqt.
if not exist "%QTDIR%\bin\windeployqt.exe" (
    for /f "delims=" %%I in ('where windeployqt.exe 2^>nul') do (
        set "QTDIR=%%~dpI.."
        goto :qtdir_ready
    )
)
:qtdir_ready
if not exist "%QTDIR%\bin\windeployqt.exe" (
    echo [ERROR] windeployqt not found. Set QTDIR to the Qt msvc2022_64 directory.
    echo         Current QTDIR=%QTDIR%
    popd >nul
    exit /b 1
)

set "CONFIG=Release"
if /i "%~1"=="Debug" set "CONFIG=Debug"

rem ---- build (reuse build.bat so toolchain detection stays in one place) ----
call "%~dp0build.bat" %CONFIG% --no-tests
if errorlevel 1 (
    echo [ERROR] Build failed.
    popd >nul
    exit /b 1
)

set "BUILD_PATH=%REPO_ROOT%\build"
if defined SNAPCOR_BUILD_DIR set "BUILD_PATH=%REPO_ROOT%\%SNAPCOR_BUILD_DIR%"

if not exist "%BUILD_PATH%\SnapOCR.exe" (
    echo [ERROR] %BUILD_PATH%\SnapOCR.exe not found.
    popd >nul
    exit /b 1
)

rem ---- read version from CMakeLists.txt (VERSION x.y.z) ----
set "VERSION="
for /f "tokens=1,2" %%A in ('findstr /r /c:"^    VERSION " CMakeLists.txt') do (
    if "%%A"=="VERSION" set "VERSION=%%B"
)
if not defined VERSION (
    echo [ERROR] Unable to read VERSION from CMakeLists.txt.
    popd >nul
    exit /b 1
)

set "DIST_ROOT=%REPO_ROOT%\dist"
set "PKG_DIR=%DIST_ROOT%\SnapOCR-%VERSION%"
set "ZIP_PATH=%DIST_ROOT%\SnapOCR-%VERSION%-win64.zip"

echo [INFO] Packaging SnapOCR %VERSION% (%CONFIG%)
if exist "%PKG_DIR%" rmdir /s /q "%PKG_DIR%"
mkdir "%PKG_DIR%" 2>nul
copy /y "%BUILD_PATH%\SnapOCR.exe" "%PKG_DIR%\" >nul
if errorlevel 1 (
    echo [ERROR] Copy SnapOCR.exe failed.
    popd >nul
    exit /b 1
)

if exist "%REPO_ROOT%\README.md" copy /y "%REPO_ROOT%\README.md" "%PKG_DIR%\README.md" >nul

echo [INFO] Running windeployqt
"%QTDIR%\bin\windeployqt.exe" --release --no-opengl-sw --no-system-d3d-compiler --no-quick-import ^
    --compiler-runtime --translations zh_CN "%PKG_DIR%\SnapOCR.exe"
if errorlevel 1 (
    echo [ERROR] windeployqt failed.
    popd >nul
    exit /b 1
)

rem ---- UI translations (snapocr_*.qm): without these the English UI is unavailable ----
if exist "%BUILD_PATH%\translations\*.qm" (
    if not exist "%PKG_DIR%\translations" mkdir "%PKG_DIR%\translations" 2>nul
    copy /y "%BUILD_PATH%\translations\*.qm" "%PKG_DIR%\translations\" >nul
    if errorlevel 1 (
        echo [ERROR] Copy UI translations failed.
        popd >nul
        exit /b 1
    )
)

echo [INFO] Creating zip
if exist "%ZIP_PATH%" del /q "%ZIP_PATH%"
rem ? Windows ?? bsdtar ???Compress-Archive ? PowerShell 5.1 ????
rem ????????????? zip???????????tar -a ???? zip?
where tar.exe >nul 2>nul
if errorlevel 1 (
    echo [ERROR] tar.exe not found. Windows 10 1803+ should provide it.
    popd >nul
    exit /b 1
)
tar -a -c -f "%ZIP_PATH%" -C "%DIST_ROOT%" "SnapOCR-%VERSION%"
if errorlevel 1 (
    echo [ERROR] tar packaging failed.
    popd >nul
    exit /b 1
)

echo [OK] Package ready:
echo      %PKG_DIR%
echo      %ZIP_PATH%
popd >nul
exit /b 0
