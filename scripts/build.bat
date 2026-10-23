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
setlocal EnableExtensions

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

rem ---- arguments ----
set "CONFIG=Debug"
set "RUN_TESTS=1"
if /i "%~1"=="Debug" set "CONFIG=Debug"
if /i "%~1"=="Release" set "CONFIG=Release"
if /i "%~1"=="RelWithDebInfo" set "CONFIG=RelWithDebInfo"
if /i "%~2"=="--no-tests" set "RUN_TESTS=0"
if /i "%~1"=="--no-tests" set "RUN_TESTS=0"

set "PATH=%QTDIR%\bin;%PATH%"

echo [INFO] Configure: config=%CONFIG% qt=%QTDIR%
"%CMAKE_EXE%" -S "%REPO_ROOT%" -B "%REPO_ROOT%\build" -G Ninja ^
    -DCMAKE_BUILD_TYPE=%CONFIG% ^
    -DCMAKE_PREFIX_PATH="%QTDIR%"
if errorlevel 1 goto :fail

echo [INFO] Build
"%CMAKE_EXE%" --build "%REPO_ROOT%\build"
if errorlevel 1 goto :fail

if "%RUN_TESTS%"=="1" (
    echo [INFO] Test
    ctest --test-dir "%REPO_ROOT%\build" --output-on-failure
    if errorlevel 1 goto :fail
)

echo [OK] Build finished: %REPO_ROOT%\build\SnapOCR.exe
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
