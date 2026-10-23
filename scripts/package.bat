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

echo [INFO] Creating zip
if exist "%ZIP_PATH%" del /q "%ZIP_PATH%"
pushd "%DIST_ROOT%" >nul
powershell -NoProfile -Command "Compress-Archive -Path 'SnapOCR-%VERSION%' -DestinationPath '%ZIP_PATH%' -Force"
if errorlevel 1 (
    echo [ERROR] Compress-Archive failed.
    popd >nul
    popd >nul
    exit /b 1
)
popd >nul

echo [OK] Package ready:
echo      %PKG_DIR%
echo      %ZIP_PATH%
popd >nul
exit /b 0
