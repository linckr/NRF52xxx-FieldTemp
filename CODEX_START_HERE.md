# CODEX_START_HERE.md

> 新的接手者（人或 AI）请**从这里开始**。目标：读完这一页就知道项目是什么、
> 当前在哪、下一步做什么、以及去读哪份文档。

---

## Project Overview

三部分构成一个完整的现场测温系统：

- **Firmware**（`NRF52xxx-FieldTemp`）：跑在 **nRF52810**（E104-BT5010A 模组）上的
  Zephyr 应用。1 Hz 采样温湿度/气压，按周期取均值写入外置 **W25Q64**，通过 BLE 上报，
  并支持**自定义 BLE OTA** 现场升级。
- **Android App**（`PandaThemperature-Android`）：配套客户端。BLE 连接与自动重连、
  实时数据、历史同步与曲线、设备配置、**固件 OTA 升级**。
- **Hardware**：E104-BT5010A 模组 + W25Q64（SPI）+ AHT30/SPL06（I2C）+ DAPLink（SWD）。

关键机制：**MCUboot 双槽**。App 把 `zephyr.signed.bin` 写进 W25Q64 的次级槽，
重启后 MCUboot 做 **ECDSA-P256 验签**，通过才覆盖主槽。

---

## Repository Paths

```
Firmware : C:\Users\linckr\NRF52xxx-FieldTemp
Android  : C:\Users\linckr\NRF52xxx-FieldTemp\android
           （统一项目的 Gradle 根目录；独立 Android 仓库 source/ 保留用于上游同步）
```

> 上面是**当前开发机**的路径。本套文档里的命令都按这些路径写成**可直接复制执行**的形式；
> 换机器/换目录时需要自行替换。仓库本身不依赖任何绝对路径（构建脚本已把本机路径全部移除）。

远端：`https://github.com/linckr/NRF52xxx-FieldTemp` 与
`https://github.com/linckr/PandaThemperature-Android`
（两者的 `upstream` 都是 yodfz 上游，**推送时注意别推错**）。

---

## 已真机验证的代码基线 commits

| 仓库 | branch | commit |
|---|---|---|
| Firmware | `main` | **`b97088d`** = 已真机验证的固件功能基线；后续文档与清理提交不代表已真机回归 |
| Android | `main` | **`be12069`** = 2026-10-09 历史/重连回归；`f86f670` 为早期基线；本轮工作区Android OTA真机验证见最新阶段记录 |

> 上述 commit 是**真机验证基线**，不是持续更新的 HEAD。当前 HEAD 请在两个仓库各自运行 `git log -1` 核对；2026-09-15 的配置/文档清理仅完成静态构建与门禁，未重新烧录真机。

固件源码版本号：**`1.0.9+0`**（来源：仓库根 `VERSION`）。
App 源码版本：`versionCode 3` / `versionName "1.1.1"`（**与固件版本无对应关系**）。

---

## Current Status

### 2026-10-09 P2 / 3000 条保留：真机升级、回读与冷启动持久性通过，小米BLE及手机精简待验收

源码版本为 `1.0.9+0`。历史 Flash 仍是 12 B/条；新增电压采用自识别 packed 存储字，
旧数据不迁移。状态能力位 `0x40` + 历史请求尾字节 `03` 才启用 14 B BLE 回传，
旧 4 B 请求保持 12 B。能力位 `0x80` + `[3000 LE32, 04]` 用于持续保留最近 3000 条，
只写 HISTORY `12340023`，绝不能写 CLEAR `12340041`。字段与安全顺序见 PROTOCOL §3。

完整原始数据已只读归档到本机，SWD 已识别；归档、数据库、凭据和私钥不入 Git。
开发签名 sysbuild / 尺寸门禁通过：App Flash **151,540 / 163,328 B**（增加 2,012 B），
签名镜像 **152,203 B**；RAM **23,080 / 24,576 B**（增加 128 B，余 **1,496 B**）；
MCUboot **31,876 / 32,768 B**（余 **892 B**）。相对 6 KiB 签名镜像预留红线余 **5,493 B**。
所有分区地址及容量未变。真实生产 C 函数的 native fixture 为 **0 failures**，Android **119** 项
单元测试通过；生产签名 APK 已验签。开发信任链新固件已真机运行并有主槽逐字节匹配证据；生产信任链尚未部署。

已完成开发信任链 1.0.9 升级与主槽逐字节回读、V3 历史回读和硬件精确保留3000条；P3已取得擦除流程、部分写入、END后和暂停搬运流程的真实电池断电恢复证据，但不证明 NOR/NVMC 忙脉冲中断电。保留策略冷启动持久性已验证；realme到小米数据库迁移已完成；尚待小米BLE/raw集合及3000条精简验收与生产信任链部署。
生产密钥已在仓库外创建；首次建立生产固件信任需通过 SWD 烧录含对应公钥的 MCUboot，
现有设备的 OTA 仍要求其当前 MCUboot 所信任的旧密钥。生产切换与数据保留需要分别验证。

**已真机验证**：BLE 实时数据、历史写入与回读、时间同步 + 墙钟整分对齐、
记录周期 60.1 s、事件限流、**OTA 完整闭环**（含断点续传、错误密钥/超容量拒绝、
MCUboot 拒绝坏签名）、电池电压端到端、全路径栈高水位。

**2026-09-14 交接批次已真机验证**（两轮连续 OTA，判据 = 主槽 `img_size`）：

| 轮次 | 推送镜像 | 主槽 `ih_img_size` | 结果 |
|---|---|---|---|
| 基线 | — | 149,528 | — |
| 1 | `build-stk8`（149,560 B） | **149,528 → 149,560** | ✅ 上传/END/TRIGGER/搬运全通过 |
| 2 | `build-verify2`（发布镜像，149,528 B） | **149,560 → 149,528** | ✅ 恢复到发布镜像，设备健康 |

两轮 `ih_ver` 都是 `1.0.8+0` —— **版本号没变、二进制变了**，正是"不能只看版本号"的实例；
判决全部依赖主槽内容。OTA 后 BLE 健康检查：状态帧 12 B、能力位 `0x3F`、实时帧 8 B、
电压 3.324 V、记录数 4167，`wallclock=0`（本次上电无手机对时，符合设计）。

**尚未验证**：
1. **电压绝对精度与电池端电压对应关系**（已确认电池供电并观察到 2.989→2.930 V，尚缺万用表对照）。
2. 小米BLE/raw集合及手机实际数据精简与生产信任链部署（数据库迁移已完成）；持续3000条冷启动持久性已通过。
3. SWD连接时出现的W25Q64 init_res=22原因未定；隔离SWD后的真实断电重试通过，不等于已定位根因。

**当前最紧的两个约束**：App RAM 余 **1,496 B**；MCUboot 余 **892 B**。

---

## Build

**Firmware**（PowerShell；必须先给签名私钥路径，脚本没有默认值）：

```powershell
$env:MCU_BOOT_SIGNING_KEY = "<仓库外的ECDSA-P256私钥路径>"
cd C:\Users\linckr\NRF52xxx-FieldTemp
C:\ncs\toolchains\66cdf9b75e\opt\bin\python.exe tools\build_sysbuild.py build-v9
```

> ⚠️ 不要直接 `west build`：PowerShell 会剥掉 `-DSB_CONFIG_BOOT_SIGNATURE_KEY_FILE="..."`
> 的内层引号，构建会以 `malformed string literal` 中止。
> ⚠️ `tools/build_sysbuild.py` 刻意**没有默认私钥路径**；没有签名私钥就不能构建发布镜像。

**Android**（PowerShell；JDK 必须是 17，Android Studio 自带的是 JDK 25，Gradle 8.13 不支持）：

```powershell
$env:JAVA_HOME = "C:\Users\linckr\.workbuddy\binaries\jdk\jdk-17.0.20.1+1"
cd C:\Users\linckr\NRF52xxx-FieldTemp\android
.\gradlew.bat :app:assembleDebug --console=plain
```

### Windows 工作站已知问题（2026-09-15 已验证）

**Firmware Git 显式 Deny ACL**

- 症状：工作树文件可编辑，但 Git 创建 `.git/index.lock` 时返回 `Permission denied`；
  `Get-Acl .git` 能看到旧沙箱 SID 的显式 `Deny` ACE。
- 根因：Codex 受限沙箱在工作区外访问仓库时留下或重新注入隔离 ACL，不是仓库对象损坏。
- 修复原则：用正常 Windows 用户权限，只移除已核对的旧 SID `Deny` 规则；不要对整个 `.git`
  执行 `icacls /reset`。写入探针通过后，先确认工作树与 `origin/main` 内容一致，再更新 HEAD / index
  和分支跟踪关系。
- 本次结果：`C:\Users\linckr\NRF52xxx-FieldTemp` 已对齐 `origin/main`，`main` 正确跟踪
  `origin/main`，工作区 clean，目标 `Deny` ACE 为 0。
- 防止复发：在 Codex 中把 Firmware 仓库本身作为可写 workspace root 打开；否则 Git 操作应在正常
  用户权限下执行。受限沙箱再次访问工作区外的 `.git`，可能重新注入隔离 ACL。

**Android Gradle / Java ZipFS `AccessDeniedException`**

- 症状：Kotlin/KSP 可以完成，但 `compileDebugJavaWithJavac` 在关闭 Gradle transform JAR 时抛
  `java.nio.file.AccessDeniedException` / `GeneratedClassCompilationException`。
- 根因：Codex 受限沙箱与 Java ZipFS 的文件访问冲突；复制 JDK、Gradle distribution 或缓存到新目录
  不能解决。该错误不表示源码、JAR 或 Gradle 缓存损坏。
- 处理：使用上面的 JDK 17，在正常 Windows 用户权限下运行：

```powershell
.\gradlew.bat clean testDebugUnitTest assembleDebug --no-daemon --console=plain
```

- 本次验证：`BUILD SUCCESSFUL`，47 个任务完成；14 个测试套件、97 项测试全部通过，
  0 failure / 0 error / 0 skipped；`app-debug.apk` 成功生成。现有 deprecated API 警告不影响构建。

---

## Flash

**首次 / 救砖：烧 `merged.hex`（含 MCUboot + 主槽）**

```powershell
$env:JAVA_HOME = "C:\Users\linckr\.workbuddy\binaries\jdk\jdk-17.0.20.1+1"
$PYOCD = "C:\Users\linckr\.workbuddy\binaries\python\envs\default\Scripts\pyocd.exe"
& $PYOCD flash -t nrf52810_xxaa --frequency 500k `
  "C:\Users\linckr\NRF52xxx-FieldTemp\build-v9\merged.hex"
```

> ⚠️ pyOCD 连接会 **halt 目标核**，读完 RAM 记得 `resume`。
> ⚠️ `pyocd erase --chip` / MCU mass erase 会清除 **nRF52810 内部 Flash**（包括 MCUboot 和 App），
> 因此正常升级流程不要使用；它**不会**自动擦除外置 W25Q64。
> 真正禁止的是**通过应用、host DFU 工具或自定义脚本对 W25Q64 整片执行 chip erase**，
> 因为 `0x28000` 以后包含 NVS 和历史数据（且没有备份）。

---

## OTA

Android App **必须发送 `zephyr.signed.bin`**（不是 `zephyr.bin`、不是 `merged.hex`）：

```
Android → BLE（服务 12340050，Control/Data/Status 三特征）
        → 写入 W25Q64 次级槽 [0x00000, 0x28000)   ← 160 KiB
        → TRIGGER → 设备重启
        → MCUboot 读次级槽 trailer → ECDSA-P256 验签
        → overwrite primary [0x08000, 0x30000)     ← 163,840 B
        → 新固件启动
```

- 验签失败 → **不搬运**，设备继续跑旧固件（overwrite-only 下不会变砖）。
- 传错文件 → 设备报 `err=6 MAGIC`；App 侧 `OtaImageParser` 会在发送前就拒掉。
- 升级发生在**外置** Flash，主槽只是被覆盖，不会碰 NVS 与历史区。

---

## Architecture Constraints

（完整清单见 `HANDOFF.md` §5，这里只列最要紧的）

1. **MCUboot 分区 = 32 KiB**，已用 31,876 B（97.28%）。**不要再往 MCUboot 加功能**。
2. **主槽 `0x08000`–`0x30000`（163,840 B）**，App 实际可用 `0x08200`–`0x30000`。
3. **次级槽 `0x00000`–`0x28000`（160 KiB，外置 W25Q64）**，**必须与主槽等大**。
4. **NVS `0x28000`–`0x2E000`**（外置）。
5. **history `0x2E000`–`0x12E000`（1 MiB，外置）**，与上面两段**不得重叠**。
6. **ECDSA-P256** 签名；换私钥 = 必须**重烧 MCUboot**。
7. **签名私钥不进仓库**：由 `MCU_BOOT_SIGNING_KEY` 或 `--signing-key` 传入。
8. **`OTA_AUTH_KEY` 不是安全边界**：它只防误触/DoS，真正拦住恶意固件的是 MCUboot 验签。
   它同样不进仓库（固件 `src/ble/ota_auth_key.h`、App `source/ota.properties`，均为 gitignore）。
9. `SPI_NOR_FLASH_LAYOUT_PAGE_SIZE` 在 App 与 MCUboot **两侧都必须是 4096**。
10. 改引脚要**同时**改 `app.overlay` 与 `sysbuild/mcuboot.overlay`。

---

## Current Next Task

**P0（已✅完成）：2026-09-14 交接批次已按 `构建 → size/release gate → 真机 OTA 回归 → 提交 → push`
的顺序落地，两轮真机 OTA 全部通过（证据见上方 Current Status）。**

**当前下一步：在小米连接模块并取得新的raw集合核对，再精简小米数据库，验证最终记录集合；数据库迁移与test APK安装已完成。生产信任链部署与P1万用表精度/负载压降对照仍待补齐；P3忙脉冲断电不在本轮已通过范围。**

2026-10-08/09 用户已切换电池供电，手机新读数约 2.989→2.930 V；先前“当前恒定3.3 V、疑似调试器供电”的描述只属于9月测试。尚不能据下降幅度推算容量或续航。必要的SWD读RAM仅作辅助，pyOCD attach会halt核，读完应resume。

P2开发信任链升级、V3回读与硬件3000条保留已验证，冷启动持久性已通过，realme到小米迁移已完成，小米BLE及手机实际精简待完成；P3四阶段流程真实去电恢复已有证据，忙脉冲断电未验证。P4 的低风险技术债
已于 2026-09-15 清理完成；详情见 `HANDOFF.md` §4。
完整任务清单（含每项的涉及文件 / 不能破坏的接口 / 验证方式 / 完成条件）见 `HANDOFF.md` §4。

---

## Read Next

| 文档 | 什么时候读 |
|---|---|
| [`HANDOFF.md`](HANDOFF.md) | **必读第二篇**。状态、任务、禁止改动项、代码地图、技术债、一致性检查 |
| [`DEVELOPMENT.md`](DEVELOPMENT.md) | 要构建 / 烧录 / 调试时（含可直接复制的 PowerShell 命令） |
| [`HARDWARE.md`](HARDWARE.md) | 要碰引脚或分区时（含完整分区表与"为什么必须写在这里"） |
| [`OTA.md`](OTA.md) | 要碰升级链路、签名、镜像格式时 |
| [`PROTOCOL.md`](PROTOCOL.md) | 要改 BLE 协议或排查两端不一致时（byte-level 表） |
| `docs/`（仓库内） | ⚠️ **上游历史设计文档**，描述的是旧方案，**不要当当前实现读** |

新接手者的第一条纪律：**文档与代码冲突时以代码为准**，并把冲突补记到 `HANDOFF.md` §12。

## 2026-10-09 Android 同步修复与统一项目

Android 已合并到本仓库 android/，直接作为 Gradle 根目录。同步来源 commit `be12069abae643085b3ed9773ce1591d462d1e2d`（独立 Android 仓库）；修复源码已在手机验证，固件本轮没有改动或重新烧录。设备状态 patch=8，不能凭版本号证明其二进制等于当前固件 HEAD。

已验证：105 项单元测试，6 项手机 Room 隔离数据库测试、1 项冷启动测试和1项显式真实 BLE 回归；连续两次全量、立即重试、传输中断连重连及增量同步均通过。00:43 有效记录 33,476（模块33,382/GPS94），重复/未来时间为0，原22,484条异常记录仍在用户授权的本机归档。手机数据库/恢复脚本不上传。

旧 APK 的 patch>=3→14B 错判造成2043/2044年；该轮设备 v8 历史仍为12B。当前源码 v9 仅通过明确能力位和请求协商14B，OTA仍 zephyr.signed.bin。修复包含无进展超时、事务提交后清缓冲、按设备/时间戳去重、会话清理互斥/断连取消、GPS进度排除及通知确认超时。冷启动会话必须在init监听前初始化。详见 [Android HANDOFF](android/HANDOFF.md)。

用户已确认电池供电，读数约2.989→2.930 V，下降59 mV；绝对精度、容量/续航和VDD到电池端压对应关系仍需万用表验证。该轮连接回归未重建固件；上面的 P2 阶段已重新构建，更新资源指标并完成生产 APK 验签，开发信任链升级与四阶段流程真实断电恢复已验证，详见最新记录；不含忙脉冲断电证明。


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
