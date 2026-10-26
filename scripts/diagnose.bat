@echo off
chcp 65001 >nul
rem SnapOCR 一键诊断：生成环境/显示器报告与多屏捕获证据，便于反馈问题。
cd /d "%~dp0"
if not exist "SnapOCR.exe" (
    echo [错误] 请把本脚本放在 SnapOCR.exe 同目录后运行。
    pause
    exit /b 1
)
echo === SnapOCR 诊断 ===
SnapOCR.exe --diagnose "%~dp0diagnose"
echo.
echo 诊断文件已写入：%~dp0diagnose
echo 请把该目录整体打包发回（doctor.txt / capture.txt / dump*.png）。
pause
