# Android 接手与同步修复（2026-10-09）

当前代码是事实源。统一项目位于 https://github.com/linckr/NRF52xxx-FieldTemp ，其中 android/ 是直接可构建的 Gradle 根目录；独立 Android 仓库保留 source/ 根目录，用于向 yodfz/PandaThemperature-Android 提交上游 PR。不要在两处各自独立修改；同步时逐文件检查差异。

## 实际修复和协议边界

设备 patch=8（1.0.8）使用 12 字节 V2 历史记录；旧安装 APK 把 patch>=3 当成 14 字节 V3，60 字节包错位解析，产生 2043/2044 年等异常数据。现有 Profile 工厂已正确，必须安装新 APK，不能只凭 versionName 判断源码版本。

本次增加连续 60 秒无记录进展才超时的监控、未来时间增量起点回退、按设备+时间戳更新模块历史（保留 GPS 行）、结束清理期间持有会话、断连失效/取消旧任务、旧包与重复 END 隔离、模块进度排除 GPS、本会话接收计数、Room 提交后才清空缓冲和实时通知确认 5 秒超时。修复新增会话字段后置造成的冷启动连接监听退出：HistorySyncSession 必须在 init 之前初始化。

BLE UUID、记录布局、OTA 命令、数据库 schema/version 均未改变。当前 V3 14 字节仍为预留功能，不能由版本号或包长度推断启用。OTA 文件仍为 zephyr.signed.bin，信任边界仍为 MCUboot ECDSA-P256，OTA auth key 仅减少误触。

## 已验证

2026-10-09 本次提交的源码（提交前基线 29a9e9a），JDK17 构建 testDebugUnitTest / assembleDebug / assembleDebugAndroidTest 成功：105 项单元测试，0 失败/错误。手机 6 项真实 Room 内存测试及 1 项冷启动测试通过；显式启用的 1 项真实 BLE 回归在 193.298 秒内完成两次连续全量、传输中断连、重连自动同步和立即增量重试，验证重复为 0、既有模块记录 ID 和 GPS 行保留。

安装的是调试签名 APK，不是生产签名发布。设备状态版本为 patch=8，未重新烧录或读取设备固件二进制 hash，不能把固件源码 HEAD 45439cf 当成此次重新烧录验证结果。

用户授权后的本机数据库恢复归档了 22,484 条旧错位记录；此恢复只针对该手机精确快照，没有加入 App 自动迁移或通用按年份删除功能。最终 00:43:05 只读核对：SQLite integrity ok，有效 33,476 条（模块 33,382、GPS 94），未来年份/模块重复均为 0，异常归档保留。数据库、APK、恢复脚本和个人信息不随提交上传。

电池供电读数从约 2.989 V 到 2026-10-09 00:43 的 2.930 V，下降约 59 mV；万用表精度、容量和续航仍未验证。手机端 OTA 完整闭环、断电矩阵及生产签名仍待完成。

## 回归入口

在 Gradle 根目录使用 JDK17：

```powershell
.\gradlew.bat testDebugUnitTest assembleDebug assembleDebugAndroidTest --no-daemon --console=plain
.\gradlew.bat connectedDebugAndroidTest -Pandroid.testInstrumentationRunnerArguments.class=com.example.pandatemperature.data.database.HistoryRecordDaoTest,com.example.pandatemperature.MainViewModelColdStartTest
```

LiveHistorySyncTest 默认跳过；仅在明确允许真实设备同步并提供 liveBle=true 时运行。它不清空任一端数据，但会连接设备、对时、下载历史和主动断连重连。冷启动测试模拟内存状态，不打开硬件连接。

正式开发不要提交 local.properties、ota.properties、keystore.properties、签名密钥或手机数据库；仅提交对应 example 模板。
