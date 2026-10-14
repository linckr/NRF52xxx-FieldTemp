# OTA.md — OTA 架构与升级链路

## 2026-10-14 最新验收：上电提示与三位电压

当前 Firmware **1.0.11+0**、生产 App **1.1.4/code6**、Room11。LINK/P0.5为实物唯一已确认可见的灯：应用启动最早阶段250 ms亮/250 ms灭，6次共3秒，再启动蓝牙。用户已确认整根SWD断开后的电池冷启动“闪烁、熄灭”，以及“连接常亮、断开熄灭”。启动软件重启也提示；MCUboot在进入应用之前失败时仍无提示，不代表电池座接触缺陷已修复。

App首页、配置、历史列表/详情、设备管理、电池指示及日志7处电压显示统一3位小数；毫伏存储/协议、Room版本和ADC测量精度未改。最终生产APK实际安装读回SHA256 `488bc447c9bc602abf4f005058a367fb4228b1a82a621f61b2fb2c41a991958c`，证书与原生产证书一致。首次DATA/P0.4版本肉眼未通过，用户确认只有一颗可见灯后改用LINK；未更改引脚定义。

最终签名主槽152362 B完整回读SHA256 `47760eb524598918ce5aefaf10060b65c2ccfe57e70b1c282bbf692f5d18bd43`。只更新primary，MCUboot、UICR不变，外部NVS/history未编程；VTOR0x8200、CFSR/HFSR0。App Flash151700 B（较1.0.10 +16）、RAM23016 B（-64，静态余1560）、MCUboot31876 B（余892，构建和设备字节均未变）。容量/版本门禁、生产公钥验签、真实C历史fixture、123 JVM及Release lint通过。

最终配对Live73.935 s通过：两次全量各3154条、部分断线保留、重连、增量重试及实时恢复。最新一致归档integrity ok：active3548、模块3184、GPS187、无GPS手机样本177；原3477条ID/核心字段、所有原手机采样与隔离记录保留，未来日期/模块重复0。回归中的反复全量是明确opt-in测试行为；测试已结束、辅助APK已卸载，App恢复正常增量同步，临时USB亮屏设置恢复0。

已知边界：当前归档有7条temperature/humidity均0的手机来源采样，模块来源无此类行；保留原数据，手机采样有效性应单独排查。首次测试发生BLUETOOTH_CONNECT权限异常，重新授权后回归通过，未声称缺失/撤回权限路径已修复。INIT22根因、ADC校准、Android13+权限与长期运行、NOR/NVMC忙脉冲断电仍待验证。本版本通过SWD部署及冷启动验证；生产OTA搬运证明来自1.0.10历史批次，本次没有重新做OTA搬运，不得混同。

正式发布：[v1.0.11](https://github.com/linckr/NRF52xxx-FieldTemp/releases/tag/v1.0.11)。完整证据见 [上电与三位电压验收](https://github.com/linckr/NRF52xxx-FieldTemp/blob/main/docs/20261014-startup-voltage-validation.md)。以下旧版本状态按历史记录理解，以本节和当前代码为准。

> 本文描述**当前实际实现**。所有体积数字取自最近一次构建（2026-09-15 clean sysbuild）。
> 协议字段级定义见 `PROTOCOL.md` §5；构建命令见 `DEVELOPMENT.md`。

---

## 1. 端到端链路

```
Android App
  │  选择 zephyr.signed.bin（含 MCUboot 头 + ECDSA-P256 签名）
  │  Control 12340051 ← START(61B: key16 + total4 + ih_ver8 + sha256_32)
  │  Data    12340052 ← 分片（MTU-3 字节/片，Write Without Response）
  │  Status  12340053 → 每 8 包通知一次「已落 Flash 的连续偏移」
  ▼
nRF52810 Application（GATT 回调只校验/拷贝/投递）
  │  系统工作队列：整槽擦除 → flash_img 流式写入 → 续传状态存 NVS
  │  END：排空缓冲 → 校验字节数 / magic / ih_ver / 整镜像 SHA-256
  │  TRIGGER：保存历史写头与配置 → boot_request_upgrade() → sys_reboot()
  ▼
W25Q64 MCUboot secondary（0x000000 – 0x028000，160 KiB）
  │  写入方式是 flash_area，分区边界即安全边界
  ▼
复位 → MCUboot
  │  读次级槽镜像头 + trailer，先验 trailer magic 判断"有无待激活升级"
  │  ECDSA-P256 验签（公钥由构建期从私钥 PEM 提取并打进 MCUboot）
  ▼
overwrite primary（0x08000 – 0x30000，163,840 B）
  │  overwrite-only：直接覆盖主槽，无 scratch、无需 slot0 搬迁
  ▼
Application（新固件启动；验签不过则不搬运，设备继续跑旧固件）
```

---

## 2. MCUboot

| 项目 | 值 | 来源 |
|---|---|---|
| 分区 | `mcuboot` `0x00000`–`0x08000` | `pm_static.yml` / `<sysbuild>/partitions.yml` |
| 分区大小 | **32,768 B（32 KiB）** | 同上 |
| **当前实际大小** | **31,876 B（97.28%）** | `<sysbuild>/mcuboot/zephyr/zephyr.bin` |
| Flash 剩余空间 | **892 B**（门禁下限 800 B，属 **WARN 但 PASS**） | `tools/size_summary.py` E 项 |
| RAM 占用 | **10,432 B / 24,576 B = 42.45%** | 构建日志 `Memory region` |
| `CONFIG_MAIN_STACK_SIZE` | 4096 | `<sysbuild>/mcuboot/zephyr/.config` |
| `CONFIG_BOOT_MAX_IMG_SECTORS` | 128 | `sysbuild/mcuboot.conf` |
| `CONFIG_UPDATEABLE_IMAGE_NUMBER` | 1 | 单向 single-image 升级 |

### 2.1 升级模式

| 配置 | 值 | 含义 |
|---|---|---|
| `SB_CONFIG_MCUBOOT_MODE_OVERWRITE_ONLY` | `y`（`sysbuild.conf`） | **overwrite-only 已启用**：次级槽 → 主槽单向覆盖，不使用 scratch |
| `CONFIG_BOOT_UPGRADE_ONLY` | `y` | 同上（生成侧读到的 `.config` 值） |
| `CONFIG_BOOT_VALIDATE_SLOT0` | `y` | **primary validation 已启用**：每次启动校验主槽签名 |
| `SB_CONFIG_PM_EXTERNAL_FLASH_MCUBOOT_SECONDARY` | `y` | 次级槽在外置 W25Q64 |
| `CONFIG_BOOT_SIGNATURE_TYPE_ECDSA_P256` | `y` | 签名算法 |

**overwrite-only 的含义**：没有 scratch 分区，升级时 MCUboot 把次级槽整块覆盖到主槽。
好处是省下 `0x28000` 的 scratch 空间与搬迁时间；代价是**升级过程中断电没有回滚能力**
（overwrite-only 的经典取舍）。本项目接受该取舍，因为升级镜像是整槽写入 + 验签通过后才搬运。

⚠️ 由于是 overwrite-only，**不涉及** `mcuboot_scratch` 分区；`pm_static.yml` 里也确实没有它。
不要在文档或代码里假设存在 scratch。

### 2.2 serial recovery

**未启用。** `<sysbuild>/mcuboot/zephyr/.config` 中不存在 `CONFIG_MCUBOOT_SERIAL` /
`CONFIG_BOOT_SERIAL_*`，`sysbuild/mcuboot.conf` 里也是 `CONFIG_SERIAL=n` / `CONFIG_UART_CONSOLE=n`。

**为什么不启用**：MCUboot 只剩 **892 B** 空间。serial recovery 会拉进 UART 驱动 +
`boot_serial` 帧协议 + 命令解析，体积远超余量。当前救砖通道是 **SWD + pyOCD**（见 `DEVELOPMENT.md`）。
**在重新评估体积之前不要打开它**（`HARDWARE.md` §4 有同样说明）。

---

## 3. 签名

### 3.1 算法与密钥

| 项目 | 值 |
|---|---|
| 算法 | **ECDSA-P256**（`SB_CONFIG_BOOT_SIGNATURE_TYPE_ECDSA_P256=y`） |
| 私钥 | 仓库外部生产 ECDSA-P256 PEM（文件名由 `--signing-key` 指定）（**不在仓库内，也不允许复制进仓库**） |
| 私钥长度 / 格式 | P-256 EC PEM |
| imgtool 版本 | **2.2.0**（NCS 3.2.1 自带） |

### 3.2 私钥如何传入构建

**唯一入口是 `tools/build_sysbuild.py`**，解析顺序：

1. 命令行 `--signing-key <path>`（也接受 `--signing-key=<path>`）
2. 环境变量 `MCU_BOOT_SIGNING_KEY`
3. 都没有 → **带用法说明报错退出**（脚本**没有**任何默认私钥路径）

脚本把路径拼成 `-DSB_CONFIG_BOOT_SIGNATURE_KEY_FILE="<path>"` 传给 west。
⚠️ 该路径**必须用正斜杠**：反斜杠会被 Kconfig/CMake 逐层吃掉，实测表现为
`C:UserslinckrDocuments...root-ec-p256.pem`（分隔符全丢），构建在签名步骤才失败且报错毫无提示。
脚本已做归一化。

> **历史方案（已废弃）**：早期版本在 `tools/build_sysbuild.py` 里硬编码了
> `C:/Users/<某开发机>/.../root-ec-p256.pem`，并在部分文档里写成"默认使用该路径"。
> 现已被上面的三步解析取代。**旧写法不得复活**——它既不可移植，也会暴露本机目录结构。

### 3.3 公钥如何进入 MCUboot

构建时 `nrf/cmake/sysbuild/image_signing.cmake` + MCUboot 的
`scripts/extract_public_key?` 流程会从私钥 PEM **提取公钥**，生成
`<build>/mcuboot/zephyr/autogen-pubkey.c` 并编进 MCUboot。因此：

- 仓库里**不存在**独立的公钥文件；
- 换私钥 = 换公钥 = **设备端 MCUboot 必须重新烧写**才认得新签名；
- 生产密钥与开发密钥的区别就是"这把 PEM 是不是同一把"。

### 3.4 签出的镜像与验证

**签名产物**：`<build>/NRF52xxx-FieldTemp/zephyr/zephyr.signed.bin`
（构建由 sysbuild 自动调用 imgtool，**不需要手工执行 `imgtool sign`**）。

| 项目 | 参考值（`build-conflict-fix`，2026-09-15） |
|---|---|
| `zephyr.bin`（裸应用） | 149,528 B |
| `zephyr.signed.bin` | **150,191 B** |
| 镜像头 | magic `0x96F3B83D`、`ih_hdr_size = 512`、`ih_img_size` @ `0x0C`、`ih_ver` @ `0x14` |
| `ih_ver` | `1.0.8+0` |

> ⚠️ 手工 `imgtool --version x.y.z` 产出的 `zephyr.signed.vN.bin` **不是发布对象**，
> `tools/release_gate.py` 的 R4 项会直接拦下来。版本号的唯一来源是仓库根 `VERSION` 文件。
> 另：ECDSA 签名是 DER 编码，长度可变（69/70 字节），所以两个构建的 `zephyr.signed.bin`
> 大小可能差 1~2 字节 —— **不要用 signed.bin 的大小做判据**，比 `zephyr.bin`。

### 3.5 MCUboot 如何验证

1. 启动时读主槽镜像头 + trailer；`CONFIG_BOOT_VALIDATE_SLOT0=y` → 每次启动都验主槽。
2. 升级时先看**次级槽 trailer magic** 判断有无待激活镜像；有则读次级槽镜像头与 TLV。
3. 用内置公钥对镜像做 ECDSA-P256 验签 + SHA-256 摘要比对。
4. 通过 → 搬运到主槽；不通过 → **不搬运，继续跑旧固件**（overwrite-only 下不会变砖）。

### 3.6 `imgtool verify` 是否实际验证过

**已实际执行**（2026-09-14）：

```
$ PYTHONPATH=C:/ncs/v3.2.1/bootloader/mcuboot/scripts \
  <ncs-python> -m imgtool.main verify --key <root-ec-p256.pem> \
  <sysbuild>/NRF52xxx-FieldTemp/zephyr/zephyr.signed.bin
Image was correctly validated
Image version: 1.0.8+0
Image digest: 58f917bd62e97bbfd33a95139e2dc82afcef4d3c60032c819b34c650c05450f7
```

负向对照（临时生成一把无关的 P-256 密钥，验证后已删除）：

```
$ ... imgtool.main verify --key <other.pem> ...zephyr.signed.bin
No signature found for the given key
```

另外，**设备端**也做过一次真机反证：把签名 TLV 翻 1 个字节造出"坏签名镜像"
（`tools/ota_mkbadsig.py`），OTA 全程通过、TRIGGER 后**主槽 `img_size` 未变**且设备健康
→ 证明 MCUboot 确实执行了验签并拒绝了它。

### 3.7 生产密钥 vs 开发密钥

- 生产 ECDSA-P256 私钥已在仓库外建立；2026-10-14 生产公钥已部署到设备 MCUboot，正式1.0.10+0通过真实电池冷启动及生产签名 OTA 搬运回读。生产公开 DER SHA256：`af13ba64a82bb8cfe0273b04ad82883596115ff76b06349ae8516c409a537512`。开发密钥不再是当前设备信任根。
- 新设备部署须用生产密钥重新构建并烧写**含对应公钥的 MCUboot** →
  之后只接受生产密钥签出的镜像。切换私有产密钥而不重烧 MCUboot，会导致所有新镜像验签失败。
- 私钥一旦丢失，无法再为已部署设备签出可用的升级镜像 —— **必须离线备份**。

---

## 4. BLE OTA

字段级定义全部在 `PROTOCOL.md` §5，此处只列"实现侧"要点。

| 项目 | 值 |
|---|---|
| 服务 UUID | `12340050-1234-5678-1234-56789abcdef0` |
| Control | `12340051`（Write，带响应） |
| Data | `12340052`（**Write Without Response**） |
| Status | `12340053`（Notify，12 B） |
| 代码 | `src/ble/ble_ota.c` / `src/ble/ble_ota.h` |
| 授权 | 16 B 预置静态密钥 `OTA_AUTH_KEY`，见 §4.4 |
| 写入缓冲 | `CONFIG_IMG_BLOCK_BUF_SIZE = 256`（= W25Q64 页大小） |
| 接收环形缓冲 | 2 × 256 B = 512 B（`ota_ring`） |
| 确认粒度 | 每 8 个 Data 包通知一次 |
| 应用侧校验 | `CONFIG_IMG_ENABLE_IMAGE_CHECK=y` → 整镜像 SHA-256 |

### 4.1 线程模型（**性能与稳定性关键**）

```
GATT 回调（BT RX 线程）            系统工作队列（sys_work_q）
─────────────────────────         ─────────────────────────────
校验长度 / 状态 / 密钥             整槽擦除（START，约 1~2 s）
拷贝分片进 512 B 环形缓冲  ──▶     Flash 写入（排空环形缓冲）
k_work_submit()                    续传状态落盘（NVS）
                                   整镜像 SHA-256（END）
                                   保存写头 + boot_request_upgrade + 重启
```

**为什么必须这样**：早期版本把擦除 / Flash 写 / NVS / SHA-256 全做在 BT RX 上下文里，
实测把 `rx_thread_stack` 推到 **968/1024（只剩 56 B）**。加栈只能"不爆"，
解决不了"长时间卡住蓝牙接收线程"。重构后 RX 峰值降到 648（空闲 376 B），
重活落到 `sys_work_q_stack`。

**背压**：Write Without Response 在 ATT 层没有背压。设备每 8 包通知一次已落盘偏移让手机限速；
环形缓冲仍被打满时**不静默丢数据**（那会产出校验不过的坏镜像），而是置 `err=11 OVERRUN`
中止会话，让手机重新 START 续传。

### 4.2 与历史/NVS 的互斥

OTA 期间 `main.c` 用 `ble_ota_in_progress()` **暂停历史落盘与 NVS 写入**。
这是**正确性要求**（三者共用同一颗 W25Q64、同一条 SPI），不只是性能优化。
守卫位置：`history_push_record()` 与 `local_nvs_flush()`。

### 4.3 断点续传

- 状态存 NVS `NVS_ID_OTA` = `{offset, total, ih_ver[8], img_id[16], valid}`。
- **判定必须同时比对 `total` + `ih_ver` + `img_id`（SHA-256 前 16 字节）**。
  只用 `(total, ih_ver)` 会把两个"同版本同大小但内容不同"的镜像错误拼接。
- 命中后续传偏移 `& ~255`（向下对齐 `IMG_BLOCK_BUF_SIZE`），**跳过整槽擦除**。
- 实现要点：`flash_img_init_id()` 的 offset 固定为 0、**不支持续传**，
  必须用 `stream_flash_init()` 显式指定起始 offset。

### 4.4 授权密钥 `OTA_AUTH_KEY`

| 项目 | 值 |
|---|---|
| 长度 | **16 字节** |
| 存储位置 | **`src/ble/ota_auth_key.h`（已被 `.gitignore` 排除，不入库）** |
| 模板 | `src/ble/ota_auth_key.h.example`（占位值 + 用法说明） |
| 缺失时行为 | `src/ble/ble_ota.h` 用 `__has_include` 检测，缺失则 **`#error` 直接中止编译** |
| 手机端对应 | `source/ota.properties`（被 `.gitignore` 排除）→ `BuildConfig.OTA_DEFAULT_AUTH_KEY` |
| 主机脚本对应 | `tools/ota_host_client.py` 的 `--key` 参数 / 环境变量 `OTA_AUTH_KEY` |

**⚠️ 它不是安全边界。** 理由：

- 该值必然存在于固件镜像、Android APK、主机脚本与开发文档中，
  攻击者可通过源码、APK 反编译或固件提取获得；
- 它防的是"附近任意手机反复擦写次级槽"这类**拒绝服务 / 误操作**，**不防逆向**；
- **真正的 OTA 安全边界是 MCUboot 的 ECDSA-P256 验签**：
  即使攻击者知道 `OTA_AUTH_KEY`，也无法让设备接受未经合法私钥签名的固件。

量产要求：**每型号/每批次使用独立密钥**，并与同批次 App 配置保持一致。

> **历史方案（已废弃）**：该密钥曾以 16 个可见 ASCII 字符的**明文**直接写在
> `src/ble/ble_ota.h`、`tools/ota_host_client.py`、Android `OtaConstants.kt` 与
> `OtaAuthKey.kt` 注释中。现已全部外置（App 侧经 `BuildConfig` 注入，测试改用样本密钥
> `SampleKey16Bytes`）。**两仓库历史中从未出现过该明文**，因此无需改写历史。
> 本套文档也刻意**不记录该密钥的取值**。

---

## 5. W25Q64 写入（OTA 视角）

### 5.1 App 如何访问 flash device

```c
#define OTA_SLOT_ID  FIXED_PARTITION_ID(mcuboot_secondary)
flash_area_open(OTA_SLOT_ID, &ota_fa);
stream_flash_init(&ota_ctx.stream, ota_fa->fa_dev, ..., ota_fa->fa_off + resume_off, ...);
flash_img_buffered_write(&ota_ctx, data, len, flush);
```

- **不硬编码地址**：走 PM 分区 ID，分区边界天然限制在 `[0x0, 0x28000)`。
  越界在分区层就被 `flash_area` API 拒绝，结构上碰不到 `0x28000` 之后的 NVS 与历史。
- 擦除：`flash_area_erase(ota_fa, 0, ota_fa->fa_size)` —— 只擦分区自身。
- 写入粒度：缓冲 256 B（W25Q64 页大小），`stream_flash` 保证不跨页。

### 5.2 地址、边界、对齐

| 项目 | 值 |
|---|---|
| secondary slot | `0x000000` – `0x028000`（163,840 B） |
| NVS | `0x028000` – `0x02E000`（24,576 B） |
| history | `0x02E000` – `0x12E000`（1 MiB） |
| 擦除单位 | 4 KiB 扇区（`CONFIG_SPI_NOR_FLASH_LAYOUT_PAGE_SIZE=4096`） |
| 写入单位 | 256 B 页 |
| 续传对齐 | 向下对齐到 `CONFIG_IMG_BLOCK_BUF_SIZE`（256 B） |

### 5.3 如何避免覆盖 NVS / history

三层保证，任一层单独都足够，叠在一起是纵深防御：

1. **分区层**：写入只经 `flash_area` 的 `mcuboot_secondary`，`fa_off + fa_size == 0x28000`，
   API 层拒绝越界。
2. **容量检查**：`ota_total > ota_fa->fa_size` → `err=3 TOO_LARGE`，**在擦除之前**拒绝。
3. **并发互斥**：OTA 期间暂停历史落盘与 NVS 写入（见 §4.2）。

Partition Manager 当前自动生成 `external_flash [0x2E000, 0x800000)` 作为剩余区；它不再与 NVS
重叠，但覆盖业务自管 history 区。因此 OTA 只能打开 `mcuboot_secondary`，不得对整个
`external_flash` 执行擦写。

### 5.4 OTA 完成后 MCUboot 如何发现镜像

`boot_request_upgrade(BOOT_UPGRADE_PERMANENT)` 会**写次级槽的 trailer magic**
（次级槽 `0x27FF0` 起）。复位后 MCUboot 读该 magic：

- magic 有效 → 认为有"待激活升级" → 读次级槽镜像头 → 验签 → 搬运到主槽 → 写入主槽 trailer。
- magic 全 `ff` → 视为无待激活升级，**不搬运**。

**判据提示**：想确认"设备上有没有待激活升级"，读次级槽 `0x0000` 处 64 B（镜像头）
与 `0x27FF0` 处 16 B（trailer），全 `ff` = 空槽。工具：
`tools/w25q64_host_dfu.py read <out> 0x0000 64`（注意该工具会 reset-and-halt，
用完必须手动 resume，见 `DEVELOPMENT.md` §6）。

---

## 6. MCUboot image format

Android OTA **必须发送 `zephyr.signed.bin`**：

| 文件 | 内容 | 能否用于 BLE OTA |
|---|---|---|
| **`zephyr.signed.bin`** | 应用镜像 + MCUboot 镜像头（512 B）+ TLV（SHA-256 / KEYHASH / **ECDSA256 签名**） | ✅ **唯一可用** |
| `zephyr.bin` | 裸应用二进制 | ❌ 无头无签名 → `err=6 MAGIC` |
| `zephyr.hex` / `merged.hex` | Intel HEX 文本；`merged.hex` 还含 MCUboot 本身 | ❌ 格式不对 |
| `dfu_application.zip` | 供 MCUboot **serial recovery** 的容器（当前未启用该模式） | ❌ |
| `zephyr.elf` | 调试用 ELF | ❌ |

**原因**：设备把收到的字节**逐字节原样**写进次级槽，不做任何改造；
MCUboot 随后按固定布局解析（magic `0x96F3B83D`、`ih_hdr_size = 512`、
`ih_img_size` @ `0x0C`、`ih_ver` @ `0x14`）并验签。
只有 `zephyr.signed.bin` 同时具备镜像头与 ECDSA 签名 TLV。

镜像尾部 TLV 类型（排错时用）：

| TLV | 含义 |
|---|---|
| `0x0010` | SHA-256 摘要 |
| `0x0001` | KEYHASH |
| **`0x0022`** | **ECDSA256 签名**（DER，`30 44 02 20 …`）⚠️ 签名**不是** `0x0001` |

---

## 7. 首次烧录 vs OTA：用哪个文件

| 场景 | 文件 | 工具 |
|---|---|---|
| **首次/恢复烧录**（产线、救砖、换 MCUboot 公钥） | **`merged.hex`**（含 MCUboot + 主槽，带绝对地址） | pyOCD / DAPLink（SWD） |
| **现场 OTA** | **`zephyr.signed.bin`** | Android App 或 `tools/ota_host_client.py` |
| 只更 MCUboot | `build-*/mcuboot/zephyr/zephyr.hex` | pyOCD（注意会动 `0x0`–`0x8000`） |

2026-09-15 clean sysbuild 参考产物：`merged.hex` 512,120 B、`dfu_application.zip` 150,997 B。

## 2026-10-14 当前生产验证

生产签名 TEST_ONLY 1.0.10+1 已经 OTA 安装并完整回读主槽152348 B匹配；随后通过 OTA 恢复正式 sysbuild 1.0.10+0（VERIFY31.533 s、TRIGGER21.822 s），最终主槽152347 B完整匹配，SHA256 `c439dfdcd5b5242a9978d7d2555e029f5bb7bb43c67d684d8e7b4a3bc0e376dd`，MCUboot未变、VTOR0x8200、CFSR/HFSR=0。临时+1镜像不发布。

正式裸App151684 B，signed152347 B、MCUboot31876 B、RAM23080 B。旧尺寸表与1.0.8反证属于历史记录。当前公开密钥 imgtool verify已通过；Android1.1.3/code5生产APK验签及小米真机安装运行均通过。INIT22未定因，详见 [生产诊断](docs/20261014-production-deployment-diagnosis.md)。

### 2026-10-14 生产 Release APK 真机验收

小米 Android 12/API31 已安装生产签名 App1.1.3/code5，无 DEBUGGABLE 标记。手机实际 APK SHA256 `41ee516f21465db426dbc2b84c917afeecb052fb275d2acc04c1c3832bbf23f1` 与发布 APK 完全相同，证书 SHA256 `43d4f1f5dda1a06948552fa1c5a440ff01c506d9333a0cf0b37c28e0592f794f`。

用户明确授权 Debug→生产签名切换。卸载前完整备份原 APK、原始数据库/WAL/SHM和设置；恢复19个文件逐字节匹配，一致SQLite SHA256 `ca667ad6e9eb59d45a70e3aa687efc29584ac6dc778ea23754d3d9ec364c48e6`。同生产签名临时辅助APK只用于恢复与验收，已卸载，不发布；手机迁移ZIP已删除，本机档案保留。

正式 Release 真机 Live98.168 s通过：初次连接及同步、连续两次全量各3062条、部分同步断线不删除已有ID、重连、立即增量重试及实时恢复。电压2.954→2.947 V为本轮ADC读数，未校准。最终SQLite integrity ok：active3363、模块3092、GPS187、无GPS手机样本84、quarantine22484，未来日期0、模块重复0。安装前3321条记录ID/核心字段均保留，所有原手机采样和隔离数据逐行匹配；3010条模块createdAt随全量同步刷新，属于现有入库元数据行为，非采样时间或核心数据改变。

正式发布入口：[v1.0.10](https://github.com/linckr/NRF52xxx-FieldTemp/releases/tag/v1.0.10)。验收未修改固件或正式APK字节。生产OTA与本轮Release运行均已有真实设备证据；Android13+权限、长期运行、INIT22实际失败根因、ADC校准及NOR/NVMC忙脉冲断电仍没有新增验证。
