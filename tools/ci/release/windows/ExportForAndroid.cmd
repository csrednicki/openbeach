@echo off
rem Packs your imported Beach Life data (userdata) into openbeach-data.zip for OpenBeach on Android (README.en.txt).
setlocal
chcp 65001 >nul
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\export_android.ps1" %*
set "OBL_EXIT=%ERRORLEVEL%"
echo.
pause
exit /b %OBL_EXIT%
