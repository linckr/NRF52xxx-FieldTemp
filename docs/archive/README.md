# 过时配置与模板

boards/*.txt为未纳入当前nrf52dk/nrf52810构建的旧板/其他SoC配置，保留原始许可。旧52832的512KB内部Flash/NVS布局不是当前布局，不能恢复套用。
templates/sample.yaml.txt为原Blinky/frdm_k64f测试模板，未代表本产品测试覆盖，已退出根测试入口。
src/sensors备选驱动、RIOT参考、PCB/模型与Word资料仍保留，不能依据文件日期猜测生产硬件版本，也不能把归档当成固件体积优化。

android/*.kt.txt 保留两个默认生成的模板测试（2+2与包名断言），已退出测试源目录；历史报告中的测试数量属于当时验收，不作为当前新测试总数。
