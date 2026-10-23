@echo off
rem ===========================================================================
rem SnapOCR build script (Windows / MSVC / Qt6 / Ninja)
rem
rem Usage:
rem   scripts\build.bat [Debug|Release|RelWithDebInfo] [--no-tests]
rem
rem Environment:
rem   QTDIR  Qt msvc2022_64 directory, default E:\Qt6\6.11.1\msvc2022_64
rem ===========================================================================
setlocal EnableExtensions EnableDelayedExpansion

set "REPO_ROOT=%~dp0.."
pushd "%REPO_ROOT%" >nul

if not defined QTDIR set "QTDIR=E:\Qt6\6.11.1\msvc2022_64"
if not exist "%QTDIR%\lib\cmake\Qt6\Qt6Config.cmake" (
    echo [ERROR] Qt6 not found. Set QTDIR to the Qt msvc2022_64 directory.
    echo         Current QTDIR=%QTDIR%
    popd >nul
    exit /b 1
)

rem ---- locate vcvars64.bat (VS 2026 / 2022, any edition incl. BuildTools) ----
set "VCVARS="
call :try_vcvars "%ProgramFiles%\Microsoft Visual Studio\18\Community"
call :try_vcvars "%ProgramFiles%\Microsoft Visual Studio\18\Professional"
call :try_vcvars "%ProgramFiles%\Microsoft Visual Studio\18\Enterprise"
call :try_vcvars "%ProgramFiles%\Microsoft Visual Studio\18\BuildTools"
call :try_vcvars "%ProgramFiles%\Microsoft Visual Studio\2022\Community"
call :try_vcvars "%ProgramFiles%\Microsoft Visual Studio\2022\Professional"
call :try_vcvars "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise"
call :try_vcvars "%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools"
call :try_vcvars "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools"

if not defined VCVARS (
    echo [ERROR] vcvars64.bat not found.
    echo         Install Visual Studio with the "Desktop development with C++" workload,
    echo         or run this script from a Developer Command Prompt.
    popd >nul
    exit /b 1
)

echo [INFO] MSVC environment: %VCVARS%
call "%VCVARS%" >nul
if errorlevel 1 (
    echo [ERROR] Failed to initialize the MSVC environment.
    popd >nul
    exit /b 1
)

rem ---- locate cmake / ninja (PATH first, then Qt tools) ----
where cmake.exe >nul 2>nul
if errorlevel 1 (
    set "CMAKE_EXE=%QTDIR%\..\..\Tools\CMake_64\bin\cmake.exe"
) else (
    set "CMAKE_EXE=cmake.exe"
)
where ninja.exe >nul 2>nul
if errorlevel 1 (
    set "PATH=%QTDIR%\..\..\Tools\Ninja;%PATH%"
)

rem ---- 构建目录（并行开发时各 Agent 用独立目录，避免 ninja 锁冲突） ----
if not defined SNAPCOR_BUILD_DIR set "SNAPCOR_BUILD_DIR=build"
set "BUILD_PATH=%REPO_ROOT%\%SNAPCOR_BUILD_DIR%"

rem ---- arguments ----
set "CONFIG=Debug"
set "RUN_TESTS=1"
if /i "%~1"=="Debug" set "CONFIG=Debug"
if /i "%~1"=="Release" set "CONFIG=Release"
if /i "%~1"=="RelWithDebInfo" set "CONFIG=RelWithDebInfo"
if /i "%~2"=="--no-tests" set "RUN_TESTS=0"
if /i "%~1"=="--no-tests" set "RUN_TESTS=0"

set "PATH=%QTDIR%\bin;%PATH%"

echo [INFO] Configure: config=%CONFIG% qt=%QTDIR% dir=%BUILD_PATH%
"%CMAKE_EXE%" -S "%REPO_ROOT%" -B "%BUILD_PATH%" -G Ninja ^
    -DCMAKE_BUILD_TYPE=%CONFIG% ^
    -DCMAKE_PREFIX_PATH="%QTDIR%"
set "RC=%ERRORLEVEL%"
if not "%RC%"=="0" (
    echo [ERROR] Configure failed with exit code %RC%.
    goto :fail
)

echo [INFO] Build
"%CMAKE_EXE%" --build "%BUILD_PATH%"
set "RC=%ERRORLEVEL%"
if not "%RC%"=="0" (
    echo [ERROR] Build failed with exit code %RC%.
    goto :fail
)

if "%RUN_TESTS%"=="1" (
    echo [INFO] Test
    ctest --test-dir "%BUILD_PATH%" --output-on-failure
    set "RC=!ERRORLEVEL!"
    if not "!RC!"=="0" (
        echo [ERROR] Tests failed with exit code !RC!.
        goto :fail
    )
)

echo [OK] Build finished: %BUILD_PATH%\SnapOCR.exe
popd >nul
exit /b 0

:try_vcvars
if defined VCVARS exit /b 0
if exist "%~1\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%~1\VC\Auxiliary\Build\vcvars64.bat"
exit /b 0

:fail
echo [ERROR] Build or test failed.
popd >nul
exit /b 1
