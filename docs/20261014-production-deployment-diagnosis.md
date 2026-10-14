# 生产部署与 INIT22 诊断说明

更新日期：2026-10-14。本说明记录已完成的证据及尚待完成的验证，不包含手机标识、用户数据库内容或私钥路径。

## 已确认的生产部署

本地正式 sysbuild 产物为 Firmware 1.0.10+0。MCUboot 内嵌 ECDSA-P256 公钥与生产公开 PEM 的 DER 完全一致，DER SHA256 为 `af13ba64a82bb8cfe0273b04ad82883596115ff76b06349ae8516c409a537512`。`zephyr.signed.bin` 与 `merged.hex` 中的 primary 已分别通过生产公开密钥验签，header/application payload 相同；独立 ECDSA 签名封装可不逐字相同。

`merged.hex` 仅含内部 Flash 地址；其 MCUboot 与生产构建逐字一致，无 UICR 或外部 Flash 地址。正式部署已完成：内部 Flash 完整回读与预期相同，UICR 未变，只有内部 Flash 被编程。部署后的 VTOR 为 `0x8200`，CFSR/HFSR 为 0。证据来自 `preflight-local.json` 与 `deployment.json`，本次说明编写未操作设备。

用户随后移除整根 SWD 和电池，等待约 5 秒，再仅接回电池；设备亮灯且手机 BLE 重新连接成功。这支持生产信任根的真实独立冷启动恢复，不能单凭这一成功认定偶发 INIT22 的根因已解决。

生产版本取消历史 3000 条上限后的全量同步已通过（32.858 秒）；记录数会继续增长。本说明不公开数据库记录或设备标识。

资源：MCUboot 31876/32768 B，Flash 余量 892 B；App Flash 151684 B、RAM 23080 B。任何增加启动诊断、重试或功能的修改仍须重新核对 Flash/RAM。新 Android 源码验证为 123 项 JVM、9 项 Room 测试通过，Debug/Release 构建通过，lint 0 errors；这些构建验证不自动等同 Release APK 真机验证。

## 生产 OTA 验证边界

本地同 payload 的生产签名 `TEST_ONLY 1.0.10+1` 已上传至 VERIFY（48.272 秒），TRIGGER 请求完成（20.464 秒）。SWD完整回读已证明主槽152348字节逐字匹配测试镜像，header为1.0.10+1、VTOR0x8200、CFSR/HFSR=0；生产签名 TEST_ONLY 1.0.10+1 已经 OTA 安装并完整回读主槽152348 B匹配；随后通过 OTA 恢复正式 sysbuild 1.0.10+0（VERIFY31.533 s、TRIGGER21.822 s），最终主槽152347 B完整匹配，SHA256 `c439dfdcd5b5242a9978d7d2555e029f5bb7bb43c67d684d8e7b4a3bc0e376dd`，MCUboot未变、VTOR0x8200、CFSR/HFSR=0。临时+1镜像不发布。

新版 Live 真机回归74.233 s通过：连续两次全量各3026条、部分断线不删除已有ID、重连后下一次10秒采样内收到历史、实时恢复；本轮未观察到旧版60秒零历史等待。最终一致归档 integrity ok：模块3056、GPS187、无GPS手机样本78、active3321、quarantine22484，未来日期0、模块重复0。该回归支持修复有效，未证明历史现场一定由同一缺陷触发。

`+1` 仅为本地可信升级测试夹具，不发布。最终公开产物应为 sysbuild 原始 `zephyr.signed.bin`（1.0.10+0），不能以夹具替代。算法、分区及 MCUboot 配置均未因本验证改变。真实固件可信边界是 MCUboot ECDSA-P256 验签；OTA auth key 不构成等价安全边界。

## INIT22 能确认什么

2026-10-09 的原内部 Flash 备份与原 MCUboot 构建 31876 B 逐字一致。ELF 确认 `0x20000148` 是 W25Q64 状态；保存的设备状态显示 clock、内部 Flash、GPIO、SPIM 初始化成功，W25Q64 的 `init_res` 为 22。Zephyr 将驱动负 errno 存成正数，所以它表示 NOR 初始化返回了 `-EINVAL`。

`PC=0x6e84` 为 fatal halt，异常帧 `PC=0x1d40` 属于 abort 的 SVC 返回地址；VTOR=0、CFSR/HFSR=0 支持 MCUboot 软件启动失败，没有证据表明是 Cortex 硬 Fault。NOR 未 ready 时，`flash_area_open()` 返回的是 **-ENODEV**，随后 MCUboot 的 open-all-flash 路径失败；不能将它误写为返回 -EINVAL。

当前生产和历史构建均启用 SPI NOR SFDP MINIMAL，PM_DEVICE 关闭，Flash layout 4096 B，标准 master/8bit/4MHz。App/MCUboot 都已有 `has-dpd` 和 `t-exit-dpd=<3000>`；属性单位是 ns，驱动向上取整后实际 sleep 1 ms。初始化发送 AB 退出深睡，检查 05 状态/WIP，再以 9F 读 JEDEC；PM 关闭时 TURN_ON 配置完成后仍先进入深睡，再由 RESUME 退出。因此不能宣称缺少唤醒命令。

## 哪些初始化分支可能产生同一个错误

本地 NCS 3.2.1 `zephyr/drivers/flash/spi_nor.c`：

- 第 1599 行：收到的 JEDEC 与预期 `EF 40 17` 不同，这是 MINIMAL configure 的唯一显式 -EINVAL。
- 第 1180 行：JEDEC 输出指针为 NULL；初始化传入栈数组，不符合该条件，而且 configure 将读取失败转换为 -ENODEV。
- TURN_ON 配置后的 `enter_dpd()`、RESUME 的 `exit_dpd()` 可直接传播底层 SPI 错误。底层 SPIM 非 master、loopback、多线、非 8 bit、频率低于 125 kHz 会返回 -EINVAL；当前静态 NOR 配置不符合这些条件。仍应捕获具体返回阶段，不能仅凭 init_res=22 排除后段失败。
- SFDP magic/容量及 page layout 的 -EINVAL 分支在 MINIMAL 初始化中不执行；运行时读写越界、擦除地址/长度不对齐及无法匹配 erase type 的 -EINVAL 也不是设备初始化错误。它们不能用来解释这次 init_res22。

## 已补充的真实诊断与尚缺证据

`old-boot-rdid-warm-reset.json` 保存了 3 次 SWD 暖 reset：每次真实 RDID SPI 返回 0，JEDEC 为 `EF 40 17`，随后 VTOR=`0x8200`。INIT22 未复现。SWD 暖 reset 会改变时序和供电条件，不能替代完全隔离的电池冷启动。

历史失败时未保存实际 JEDEC 三字节、SR/WIP 或 VDD/SPI 波形。SWD reset 后恢复，以及整根 SWD 拔除后的冷启动成功，都不足以定因。SWD 反向供电、Flash 上电时序或残留深睡状态仍只是待验证候选；RESETREAS=0 也不能独立证明彻底掉电。

如需可靠定因，优先用高阻采集 CS28/SCK30/MOSI31/MISO29 与 VDD，保留掉电最低电压、上电时间以及 AB/05/9F 实际帧、SR/WIP、返回 ID。采集设备不可反向供电；SWD 断开条件与接入调试条件应分别记录。断点暂停会改变时序，属于第二层证据。

## 当前生产 ELF 的诊断断点

已离线重新核对 `build-unlimited-production/mcuboot/zephyr/zephyr.elf`，地址恰与历史构建相同，但这是本次核对结果，不能当作所有以后构建的固定地址：

| 地址 | 读取内容 |
| --- | --- |
| `0x5a38` | 初始 exit-DPD 后的 r0 返回码 |
| `0x5a42` | 初始 RDSR 后的 r0，正值为 SR，负值为错误 |
| `0x5a60` | RDID 后 r0；sp+12 的 3 字节为实际 JEDEC |
| `0x5a70` | memcmp 后 r0，0 表示 ID 相同 |
| `0x5b86` | 明确 JEDEC mismatch 的 -EINVAL 分支 |
| `0x5af4` | TURN_ON 配置成功后 enter-DPD 返回码 |
| `0x5b22` | RESUME exit-DPD 返回码，检查尚未执行的 r0 |

地址只供后续获授权诊断参考，诊断文档整理不操作设备；此前已有三次暖重置RDID断点取证，均未复现错误。不同构建须再次验证 ELF 与设备 boot 字节一致。

## 最小修复原则与结论

INIT22 **未复现、根因未定**，本轮未据猜测修改驱动。若波形证明上电/唤醒时序不足，可评估有限延时或一次只读 JEDEC 重试；若证明具体 SPI 参数错误，应只修该参数；若证明 SWD 反向供电，应优先修供电/接线。均须评估剩余 892 B MCUboot Flash 及 RAM，不宜开启全量日志。

不应忽略 ID mismatch、绕过外部 Flash 初始化或验签，不应为这个错误随意修改 Flash layout、分区或协议。之前 P3 四个逻辑阶段断电恢复通过仍不证明 NOR/NVMC WIP 脉冲期间精确断电。
