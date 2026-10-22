# 文档索引

当前开发状态、风险及任务统一见[开发入口](../CODEX_START_HERE.md)，项目介绍与下载见[README](../README.md)。代码是实现事实源；各验收报告只证明对应版本、设备与测试范围。

## 开发手册

- [环境、构建与调试](guides/DEVELOPMENT.md)
- [硬件、引脚与 Flash 分区](guides/HARDWARE.md)
- [OTA 架构与升级链路](guides/OTA.md)
- [Firmware 与 Android 协议](guides/PROTOCOL.md)
- [Android 工程说明](../android/README.md)

## 候选方案与验证记录

以下为现有记录，不代表本次文档整理重新进行了设备验收。

- [生产 OTA、离线校时与栈风险实测（2026-10-16）](20261016-生产OTA与离线校时实测.md)
- [可靠性修复验证与剩余验收](20261016-可靠性修复验证.md)
- [离线时间机制与兼容](20261016-离线时间机制.md)
- [可靠性收尾计划](20261016-离线时间与可靠性收尾-plan.md)
- [OTA 精确断电与诊断待验收](20261016-OTA精确断电与诊断待验收.md)
- [候选固件硬件验收顺序](20261016-候选固件硬件验收顺序.md)
- [已发布 1.0.11 的历史验收](20261014-startup-voltage-validation.md)

## 硬件与产品资料

- [模组数据手册](datasheets/E104-BT5010A+datasheet_CN_v2.0.pdf)
- [硬件照片一](images/hardware/20260116-104604.jpg)、[硬件照片二](images/hardware/20260116-104614.jpg)
- [PCB 与 BOM](../PCB/)、[外壳模型](../3DModel/)、[原使用说明 Word 资料](../说明文档/)

原硬件、模型和 Word 资料的生产版次尚未统一确认，不能直接代替当前代码中的 GPIO、分区或协议定义。

## 历史与归档

- [2026-10-23 整理前交接](history/20261023-HANDOFF.md)
- [2026-10-14 验收摘要（从三份手册集中保存）](history/20261014-验收摘要.md)
- [2026-10-15 旧开发入口](history/20261015-CODEX_START_HERE.md)、[旧交接](history/20261015-HANDOFF.md)
- [过时配置与模板](archive/README.md)、[旧说明入口](archive/README.rst)
- [历史工具原文与适用边界](../tools/legacy/README.md)
- [旧 Web BLE 控制台](../index.html)：仍留原路径；仅供旧协议参考。
- [前次整理结果](20261015-过时内容整理结果.md)、[本次归整计划](20261023-根目录与文档归整-plan.md)、[归整结果](20261023-根目录与文档归整结果.md)

本目录保留的旧设计、选型和专题文档供追溯使用，不能从中恢复旧 UUID、旧引脚、旧容量或旧烧录命令。历史数值、测试次数和发布状态须结合日期阅读。
