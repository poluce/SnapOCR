@echo off
chcp 65001 >nul
rem SnapOCR 一键诊断：生成显示器布局与多屏捕获证据，便于反馈问题。
cd /d "%~dp0"
if not exist "SnapOCR.exe" (
    echo [错误] 请把本脚本放在 SnapOCR.exe 同目录后运行。
    pause
    exit /b 1
)
echo === SnapOCR 诊断 ===
echo 1/2 生成 doctor.txt ...
SnapOCR.exe --doctor > doctor.txt 2>&1
echo 2/2 生成 dump.png / dump.m0.png / dump.m1.png ...
SnapOCR.exe --capture-dump dump.png > capture.txt 2>&1
echo.
echo 已生成：doctor.txt  capture.txt  dump.png  dump.m0.png  dump.m1.png
echo 请把本目录下的这些文件（以及 %%LOCALAPPDATA%%\poluce\SnapOCR\logs 的最新日志）一起反馈。
pause
