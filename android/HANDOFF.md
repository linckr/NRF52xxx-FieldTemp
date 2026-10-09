# Android 接手与同步修复（2026-10-09）

当前代码是事实源。统一项目位于 https://github.com/linckr/NRF52xxx-FieldTemp ，其中 android/ 是直接可构建的 Gradle 根目录；独立 Android 仓库保留 source/ 根目录，用于向 yodfz/PandaThemperature-Android 提交上游 PR。不要在两处各自独立修改；同步时逐文件检查差异。

## 实际修复和协议边界

设备 patch=8（1.0.8）使用 12 字节 V2 历史记录；旧安装 APK 把 patch>=3 当成 14 字节 V3，60 字节包错位解析，产生 2043/2044 年等异常数据。现有 Profile 工厂已正确，必须安装新 APK，不能只凭 versionName 判断源码版本。

本次增加连续 60 秒无记录进展才超时的监控、未来时间增量起点回退、按设备+时间戳更新模块历史（保留 GPS 行）、结束清理期间持有会话、断连失效/取消旧任务、旧包与重复 END 隔离、模块进度排除 GPS、本会话接收计数、Room 提交后才清空缓冲和实时通知确认 5 秒超时。修复新增会话字段后置造成的冷启动连接监听退出：HistorySyncSession 必须在 init 之前初始化。

上述连接修复批次未改变BLE UUID、记录布局、OTA命令或数据库schema/version。后续P2已增加明确能力协商：0x40启用V3的5 B请求、0x80启用保留命令，仍不能由版本号或包长度推断。OTA 文件仍为 zephyr.signed.bin，信任边界仍为 MCUboot ECDSA-P256，OTA auth key 仅减少误触。

## 已验证

2026-10-09 本次提交的源码（提交前基线 29a9e9a），JDK17 构建 testDebugUnitTest / assembleDebug / assembleDebugAndroidTest 成功：105 项单元测试，0 失败/错误。手机 6 项真实 Room 内存测试及 1 项冷启动测试通过；显式启用的 1 项真实 BLE 回归在 193.298 秒内完成两次连续全量、传输中断连、重连自动同步和立即增量重试，验证重复为 0、既有模块记录 ID 和 GPS 行保留。

安装的是调试签名 APK，不是生产签名发布。设备状态版本为 patch=8，未重新烧录或读取设备固件二进制 hash，不能把固件源码 HEAD 45439cf 当成此次重新烧录验证结果。

用户授权后的本机数据库恢复归档了 22,484 条旧错位记录；此恢复只针对该手机精确快照，没有加入 App 自动迁移或通用按年份删除功能。最终 00:43:05 只读核对：SQLite integrity ok，有效 33,476 条（模块 33,382、GPS 94），未来年份/模块重复均为 0，异常归档保留。数据库、APK、恢复脚本和个人信息不随提交上传。

电池供电读数从约 2.989 V 到 2026-10-09 00:43 的 2.930 V，下降约 59 mV；万用表精度、容量和续航仍未验证。该批次的手机端OTA与真实断电矩阵待完成；生产签名的新阶段状态见下节。

## 回归入口

在 Gradle 根目录使用 JDK17：

```powershell
.\gradlew.bat testDebugUnitTest assembleDebug assembleDebugAndroidTest --no-daemon --console=plain
.\gradlew.bat connectedDebugAndroidTest -Pandroid.testInstrumentationRunnerArguments.class=com.example.pandatemperature.data.database.HistoryRecordDaoTest,com.example.pandatemperature.MainViewModelColdStartTest
```

LiveHistorySyncTest 默认跳过；仅在明确允许真实设备同步并提供 liveBle=true 时运行。它不清空任一端数据，但会连接设备、对时、下载历史和主动断连重连。冷启动测试模拟内存状态，不打开硬件连接。

正式开发不要提交 local.properties、ota.properties、keystore.properties、签名密钥或手机数据库；仅提交对应 example 模板。

## P2 / 持续3000条与生产Release阶段（设备回读与冷启动持久性通过，小米BLE及手机精简待验收）

当前源码versionCode3 / versionName1.1.1；119项单元测试通过，生产签名Release APK已验签。
生产Release APK尚未安装；手机仍Debug APK，开发信任链1.0.9已升级并逐字节验证。

状态byte5 bit6(0x40)显式选V3；HISTORY请求为timestamp LE32加03，14 B布局是V2加uint16 mV。
无该位仍V2/旧ESS V1；旧存量14 B回读时电压FFFF代表未知，不用当前电压回填。
bit7(0x80)支持HISTORY请求[3000 LE32,04]，当前固件仅支持该数量。
维护期间状态byte4 bit2置位，须等其清除并核对HISTORY_INFO总数/起点，再全量回读。
任何保留失败均不得回退到CLEAR特征，否则旧固件会全量清空。

维护真机测试默认跳过，只有明确授权及显式maintenanceAction参数才运行。
archive_sync是先完整同步供本机归档；retain才发设备保留命令，不等于手机数据库已删除旧数据。
手机数据库裁剪须在完整快照留档后单独执行事务，并验证保留3000条及原记录一致性。
本机完整原始档案已生成，硬件精确保留3000条已验证；手机副本集合验证通过但未应用，原手机数据库保留；私人数据库、归档、APK与凭据不上传Git。

生产keystore与ECDSA密钥均在仓库外。Android通过PANDA_RELEASE_KEYSTORE_PROPERTIES读取外置配置，
只记录变量名，不记录真实凭据路径或内容。生产APK签名不改变MCUboot信任：首次生产公钥部署需SWD，
当前设备OTA必须仍使用其已有公钥对应的签名私钥。

Firmware v9开发构建Flash151540 B / signed152203 B / RAM23080 B，相对v8增加Flash2012 B、RAM128 B；RAM余1496 B，MCUboot余892 B，分区保持原布局。
native生产C故障注入0 failures；P2电压/硬件保留/raw集合及P3四阶段流程边界真实去电已验证，冷启动保留持久性已验证；忙脉冲断电与生产固件部署仍待验证，数据库迁移已完成，小米BLE及手机精简待验收。


### 2026-10-09 OTA / P2 真机阶段记录（冷启动通过，小米BLE及手机精简待验收）

手机使用正式 Android OTA 代码上传开发信任链的 1.0.9 `zephyr.signed.bin`（152,203 B）。升级前设备 App 和 MCUboot 字节匹配 `build-v8`；内部 Flash、UICR、外部 secondary/NVS/history 与完整手机数据库均已本机归档，不入 Git。

| P3 场景 | 已取得的证据与结果 | 边界 |
|---|---|---|
| END 后、TRIGGER 前 | 初次电池断电后主槽163,840 B不变。隔离整根SWD后重复断电，BLE及同镜像续传VERIFY通过（25.588 s） | 初次SWD仍连接的后续启动曾出现W25Q64 init_res=22；原因未定，保留风险 |
| 部分写入 | confirmed=32,768/152,203 B、未END/TRIGGER时真实电池断电；续传VERIFY通过（37.018 s） | 证明部分上传期间断电恢复，不证明SPI写忙脉冲被切断 |
| 擦除流程 | 第二次断电后secondary前143,360 B已擦除、镜像尾部8,192 B仍保留；重新上传VERIFY通过（48.926 s） | 证明擦除流程中断，不证明NOR WIP脉冲中断 |
| MCUboot搬运 | copy3快照前1,024 B匹配候选、完整镜像未完成；CPU暂停后用户拔整根SWD和电池5 s；重启主槽候选152,203 B逐字节匹配，MCUboot32 KiB不变，1.0.9、VTOR=0x8200、CFSR/HFSR=0 | 调试器暂停的搬运流程遭遇真实去电，不等于NVMC写脉冲中断 |

升级后完整历史同步299.778 s通过，实际接收33,288条；已有传感器核心/GPS保护、无重复和无未来时间检查通过。能力字节255（0xFF）；9条历史电压为2.851–2.876 V，实时VM读数2.871 V（VM状态，不作为fresh raw证明）。旧存量电压未知，不用当前电压回填。

硬件保留测试90.353 s通过：维护busy清除、设备count=3000、全量回读3000。独立BLE raw捕获38.416 s通过：3000条V3、42,000 B、真实END、前后8 B HISTORY_INFO一致。手机数据库副本已按这些raw记录精确验证3000条，保留GPS180条及quarantine22,484条；**副本尚未应用到手机，当前手机仍为保留原数据库的Debug APK**。

旧1.0.8测试序列中硬件尾部14条被擦除/覆盖；这些记录的timestamp及传感器数值14/14存在于本机完整App归档。原档NVS写头99:14、擦除后快照99:3，oldest均0；事故瞬间检查点未知，不能断言为0。旧scan在检查点落后、恢复被触发时漏掉next_sector的部分数据，与该损失一致。实际C回归覆盖stale99:0+14及stale99:14+28：旧函数回退99:0，新函数分别恢复99:14/99:28、追加后全部原行保留且零擦除。新策略冷启动持久性现已通过隔离SWD后的真实电池断电、全量回读及独立raw验证。

生产ECDSA及Android签名资产在仓库外，生产APK验签通过；**生产MCUboot公钥信任尚未部署、生产APK尚未安装**。当前OTA沿用设备原公钥对应的开发信任链。GitHub Release尚未发布；两个upstream PR #2在本轮记录时OPEN、未合并，后续须实时查询。数据库迁移及小米test APK安装已完成；下一步是小米BLE连接、重新raw捕获核对，再应用3000条精简。精简尚未应用，不能写为通过。

### 2026-10-10 接手状态更新（证据采集于前一日晚间）

硬件保留后，realme端已在隔离整根SWD、实际电池断电后完成冷启动验证：archive_sync 23.296 s通过，hardware count=3000、完整回读3000，传感器核心/GPS保护、无未来时间和无重复检查通过。再次独立raw捕获19.434 s通过：3000条V3、42,000 B、真实END、前后8 B HISTORY_INFO稳定。保留策略冷启动持久性已验证。最近一次VDD读数2.765 V来自2026-10-09约18:45，不是2026-10-10当前实时测量，精度仍待万用表对照。

用户已授权将realme历史迁移到小米，不保留小米原有数据。小米test APK已覆盖安装。realme完整数据库已迁移到小米，迁移后的主库SHA与完整源归档一致，App启动成功；保留源模块33,446条、GPS187条及quarantine22,484条，小米原有1,042条GPS已单独本机备份、未并入。此前小米1.1.0 BLE已通过；新版1.1.1来源修复真机回归因AOD尚未开始，详见最新记录。手机3000条精简尚未应用；需先在小米连接当前模块，再取得新的raw历史集合并核对后应用精简。 私人档案路径、手机序列号和数据不写入Git。

签名产物区分：已真机回读匹配的是开发信任链候选152,203 B；当前本地生产release asset为152,202 B。ECDSA DER签名长度可变，不能把生产镜像当成已安装开发候选或声称完整signed文件逐字节一致。资源上界仍按152,203 B记录。生产MCUboot公钥信任尚未部署，生产APK未安装，GitHub Release未发布；两个upstream PR #2在本轮记录时仍OPEN，后续须实时核对。

开发与生产镜像的App payload均为151,540 B，已逐字节一致，SHA256为 `236323e4319f7228ce6b4856bb1736a4bede1cf58e3dfc85239678bdc943494c`；生产App代码与已真机验证的开发payload相同。生产 `imgtool verify` 通过、版本1.0.9；Android生产APK的v2/RSA3072验签通过。这些不代表生产MCUboot公钥已经部署或生产APK已经安装。

### 小米迁移最新状态

小米test APK已覆盖安装。realme完整数据库已迁移到小米，迁移后的主库SHA与完整源归档一致，App启动成功；保留源模块33,446条、GPS187条及quarantine22,484条，小米原有1,042条GPS已单独本机备份、未并入。此前小米1.1.0 BLE已通过；新版1.1.1来源修复真机回归因AOD尚未开始，详见最新记录。手机3000条精简尚未应用；需先在小米连接当前模块，再取得新的raw历史集合并核对后应用精简。

### 2026-10-10 App 1.1.1 来源隔离修复（精简尚未执行）

当前App源码为versionCode3 / versionName1.1.1，Room数据库版本11。新增 `TemperatureRecord.isPhoneSample`（SQL INTEGER NOT NULL DEFAULT 0）：手机实时落盘始终标记为true，即使无GPS；硬件历史upsert、增量起点和计数只处理false且无坐标的记录。10→11迁移无损增加字段，并将已有GPS行标为phone；旧无GPS行无法确定来源，保守默认false继续兼容历史候选，后续须在完整归档保护下按精确硬件raw集合过滤，不能凭年份/无GPS批量猜测删除。

定位到一条旧无GPS手机实时行与硬件历史传感器核心不同，严格保护断言已阻止3000条精简，数据库精简尚未应用。修复保留严格核心数值一致性断言，不通过放宽精度掩盖差异；第一次同步可能更新该旧行并触发保护失败，须核对差异已消除后再做稳定重复回归。

构建证据：119项单元测试0失败，Release lint/build通过，1.1.1生产APK v2/RSA3072验签通过且证书未变。小米8项isolated Room测试（7项DAO、1项真实SQLite10→Room11迁移）8.478 s通过；覆盖无GPS手机行与历史同timestamp不被upsert覆盖、历史增量/计数排除phone，以及迁移记录/设备/隔离表无损。

此前小米1.1.0真实BLE archive_sync 79.289 s通过，硬件3000/全量3000、核心和GPS保护、无重复/未来时间、cap0xFF/v9；迁移主库先与源归档SHA一致。该次同步后模块33,458/GPS187（新来源字段引入前的无GPS分类，不等于确证硬件行数），VM电压2.875 V来自2026-10-10约00:17的观测。升级1.1.1后首轮真实archive_sync因锁屏AOD导致Activity未在30 s内RESUMED而未开始，无FATAL；正在等待用户解锁并置前台。不能把旧版BLE通过当作新版来源修复的真机回归通过。手机3000条精简仍待新版稳定同步、fresh raw集合及最终数据库核对；生产APK未安装、生产固件信任未部署。
