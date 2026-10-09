# Android 客户端

本目录是直接可构建的 Gradle 根目录，合并来源为 [linckr/PandaThemperature-Android](https://github.com/linckr/PandaThemperature-Android/commit/be12069abae643085b3ed9773ce1591d462d1e2d) 的 source/。该commit为合并基线；当前P2/保留改动在统一项目继续开发，向独立仓库回传时须重新核对；不包含 .git、构建产物、手机数据、私有配置或源码压缩包。

使用 JDK17 和本机 Android SDK，在本目录运行 `gradlew.bat testDebugUnitTest assembleDebug`。SDK 路径可由 ANDROID_HOME 或本地忽略的 local.properties 设置。OTA/签名凭据通过 ota.properties / keystore.properties 注入，模板已提供；不可提交真实文件。

接手与真机回归见 [HANDOFF.md](HANDOFF.md)，Firmware/Android 公共协议和架构见根目录 [CODEX_START_HERE.md](../CODEX_START_HERE.md) 与 [PROTOCOL.md](../PROTOCOL.md)。后续开发以本统一项目为入口；向独立 Android 上游回传时同步对应 source/ 文件，不在两份副本各自开发。

当前源码versionCode3 / versionName1.1.1。历史14 B仅由状态0x40能力及5 B显式请求协商，
旧固件仍12 B；0x80支持HISTORY特征的持续3000条保留。不要按版本号/包长猜测，也不要向CLEAR发送保留命令。
原始数据留档后，手机数据库与模块裁剪是分别授权、分别验证的操作，不在连接时自动删除。
119项单测与生产APK验签通过；开发信任链1.0.9主槽回读、硬件保留3000条及独立V3 raw集合验证通过。P3四阶段流程断电已有恢复证据，不证明忙脉冲断电。冷启动保留持久性已验证；realme数据库已迁移到小米且test APK已安装，历史迁移时Debug 1.1.0启动成功（当前源码/安装已更新为1.1.1）；此前小米1.1.0 BLE已通过；新版1.1.1回归待解锁前台，手机3000条精简未应用；生产信任尚未部署。
生产签名配置通过PANDA_RELEASE_KEYSTORE_PROPERTIES读取仓库外文件；实际凭据及手机备份不得入Git。

realme隔离SWD后的实际断电冷启动与再次raw3000条回读通过。realme完整数据库已迁移到小米、主库SHA匹配源归档，旧小米GPS留档未并入，Debug App及test APK已安装；此前小米1.1.0 BLE已通过，新版1.1.1回归待解锁前台，3000条精简未应用。DEV签名候选152203 B已验收，生产asset152202 B未部署；DER长度可变，不能当作同一镜像。

开发与生产镜像的App payload均为151,540 B，已逐字节一致，SHA256为 `236323e4319f7228ce6b4856bb1736a4bede1cf58e3dfc85239678bdc943494c`；生产App代码与已真机验证的开发payload相同。生产 `imgtool verify` 通过、版本1.0.9；Android生产APK的v2/RSA3072验签通过。这些不代表生产MCUboot公钥已经部署或生产APK已经安装。

### 小米迁移最新状态

小米test APK已覆盖安装。realme完整数据库已迁移到小米，迁移后的主库SHA与完整源归档一致，App启动成功；保留源模块33,446条、GPS187条及quarantine22,484条，小米原有1,042条GPS已单独本机备份、未并入。此前小米1.1.0 BLE已通过；新版1.1.1来源修复真机回归因AOD尚未开始，详见最新记录。手机3000条精简尚未应用；需先在小米连接当前模块，再取得新的raw历史集合并核对后应用精简。

### 2026-10-10 App 1.1.1 来源隔离修复（精简尚未执行）

当前App源码为versionCode3 / versionName1.1.1，Room数据库版本11。新增 `TemperatureRecord.isPhoneSample`（SQL INTEGER NOT NULL DEFAULT 0）：手机实时落盘始终标记为true，即使无GPS；硬件历史upsert、增量起点和计数只处理false且无坐标的记录。10→11迁移无损增加字段，并将已有GPS行标为phone；旧无GPS行无法确定来源，保守默认false继续兼容历史候选，后续须在完整归档保护下按精确硬件raw集合过滤，不能凭年份/无GPS批量猜测删除。

定位到一条旧无GPS手机实时行与硬件历史传感器核心不同，严格保护断言已阻止3000条精简，数据库精简尚未应用。修复保留严格核心数值一致性断言，不通过放宽精度掩盖差异；第一次同步可能更新该旧行并触发保护失败，须核对差异已消除后再做稳定重复回归。

构建证据：119项单元测试0失败，Release lint/build通过，1.1.1生产APK v2/RSA3072验签通过且证书未变。小米8项isolated Room测试（7项DAO、1项真实SQLite10→Room11迁移）8.478 s通过；覆盖无GPS手机行与历史同timestamp不被upsert覆盖、历史增量/计数排除phone，以及迁移记录/设备/隔离表无损。

此前小米1.1.0真实BLE archive_sync 79.289 s通过，硬件3000/全量3000、核心和GPS保护、无重复/未来时间、cap0xFF/v9；迁移主库先与源归档SHA一致。该次同步后模块33,458/GPS187（新来源字段引入前的无GPS分类，不等于确证硬件行数），VM电压2.875 V来自2026-10-10约00:17的观测。升级1.1.1后首轮真实archive_sync因锁屏AOD导致Activity未在30 s内RESUMED而未开始，无FATAL；正在等待用户解锁并置前台。不能把旧版BLE通过当作新版来源修复的真机回归通过。手机3000条精简仍待新版稳定同步、fresh raw集合及最终数据库核对；生产APK未安装、生产固件信任未部署。
