@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\install-windows.ps1"
set "NETSPRO_INSTALL_EXIT=%ERRORLEVEL%"
if not "%NETSPRO_INSTALL_EXIT%"=="0" echo Installation failed. Read the error above, then run install.bat again.
echo.
pause
exit /b %NETSPRO_INSTALL_EXIT%
