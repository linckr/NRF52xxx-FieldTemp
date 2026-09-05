@echo off
REM 编译固件到 nRF52832 板子 (NCS 3.2.1)
REM 使用 boards/nrf52832dk_nrf52832_cpuapp.overlay

set BUILD_DIR=build_nrf52832
set BOARD=nrf52832dk_nrf52832

echo Building for %BOARD% ...
west build -b %BOARD% --build-dir %BUILD_DIR%
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo.
echo Build done. Flash with: west flash -d %BUILD_DIR%
