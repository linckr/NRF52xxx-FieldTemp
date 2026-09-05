@echo off
REM nRF52832 SPI Flash 测试脚本

echo ========================================
echo nRF52832 SPI Flash 配置检查
echo ========================================
echo.

echo [1/4] 清理旧的编译文件...
if exist build_nrf52832 (
    rmdir /s /q build_nrf52832
)

echo.
echo [2/4] 编译固件...
west build -b nrf52832dk_nrf52832 --build-dir build_nrf52832
if %ERRORLEVEL% neq 0 (
    echo 编译失败！
    pause
    exit /b 1
)

echo.
echo [3/4] 检查 SPI 配置...
echo.
echo --- Kconfig SPI 配置 ---
findstr /C:"CONFIG_SPI" build_nrf52832\zephyr\.config
echo.

echo --- 设备树 SPI1 配置 ---
findstr /C:"spi@40004000" build_nrf52832\zephyr\zephyr.dts
echo.

echo [4/4] 烧录固件...
echo 请确认开发板已连接，按任意键继续烧录...
pause > nul
west flash -d build_nrf52832

echo.
echo ========================================
echo 烧录完成！
echo ========================================
echo.
echo 请打开串口监视器 (115200 8N1) 查看日志
echo 关键日志:
echo   [W25Q64] 使用 SPI1 (nRF52832)
echo   [W25Q64] SPI设备已就绪
echo   [W25Q64] JEDEC ID: 0xEF 0x40 0x17
echo.
pause
