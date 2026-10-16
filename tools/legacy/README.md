# 历史工具（仅阅读）

本目录txt保留原始内容和来源，不是可执行发布入口。

- verify_v6_timing.py.txt使用build-stk6固定RAM地址，不能在当前固件运行。需要复用时必须读取当前ELF并单独审查写入范围。
- 两份nRF52832批处理属于旧板、旧west构建和烧录流程，不能用于当前nRF52810/MCUboot；test_spi_flash包含清目录和烧录操作。
- 当前用tools/build_sysbuild.py构建，正式入口见根DEVELOPMENT。verify_v7_wallclock等动态取ELF的工具不因名字旧就判废；它们仍有运行扰动，使用前检查适用性。
