# Android 客户端

统一仓库android/与独立仓库source/均是Gradle根目录。本README在独立仓库根目录；本地交接文件随对应布局同步，公共入口使用canonical链接：[CODEX_START_HERE](https://github.com/linckr/NRF52xxx-FieldTemp/blob/main/CODEX_START_HERE.md)、[PROTOCOL](https://github.com/linckr/NRF52xxx-FieldTemp/blob/main/PROTOCOL.md)。

当前App **1.1.3/code5**、Room11。JDK17+本机SDK构建；SDK路径/OTA授权/签名凭据只在本机注入，模板不是凭据，私钥和手机档案不提交。123 JVM、9串行Room、Debug/testAPK/生产Release及完整lint通过（零错误）；生产APK证书不变，手机运行Debug。

已取消App固定3000精简及强制全量，正常增量保留全部模块历史；isPhoneSample保护有/无GPS手机行，V3历史电压保持。当前cap0x80维护只发HISTORY[0LE32,04]，必须ACK+idle确认，旧v9拒绝即明确失败，不写CLEAR或回退3000。历史通知确认DISABLE→request→ENABLE，旧Gatt回调不改新会话。

生产boot及正式1.0.10+0已SWD完整回读、UICR未改，独立电池冷启动连接成功。有效硬件容量64848（一个备用扇区），取消+全量32.858s通过：硬件3010/App模块3039/手机样本252，已超过3000。生产签名 TEST_ONLY 1.0.10+1 已经 OTA 安装并完整回读主槽152348 B匹配；随后通过 OTA 恢复正式 sysbuild 1.0.10+0（VERIFY31.533 s、TRIGGER21.822 s），最终主槽152347 B完整匹配，SHA256 `c439dfdcd5b5242a9978d7d2555e029f5bb7bb43c67d684d8e7b4a3bc0e376dd`，MCUboot未变、VTOR0x8200、CFSR/HFSR=0。临时+1镜像不发布。

新版 Live 真机回归74.233 s通过：连续两次全量各3026条、部分断线不删除已有ID、重连后下一次10秒采样内收到历史、实时恢复；本轮未观察到旧版60秒零历史等待。最终一致归档 integrity ok：模块3056、GPS187、无GPS手机样本78、active3321、quarantine22484，未来日期0、模块重复0。该回归支持修复有效，未证明历史现场一定由同一缺陷触发。

INIT22三暖重置未复现、JEDEC/rc正常，但根因未知；ADC未校准，旧P3不证明忙脉冲断电。详细当前记录见交接文档；旧App1.1.2/3000回归明确为历史，不作为当前限制或新版Live验收。

### 2026-10-14 生产OTA主槽回读

生产签名 TEST_ONLY 1.0.10+1 已经 OTA 安装并完整回读主槽152348 B匹配；随后通过 OTA 恢复正式 sysbuild 1.0.10+0（VERIFY31.533 s、TRIGGER21.822 s），最终主槽152347 B完整匹配，SHA256 `c439dfdcd5b5242a9978d7d2555e029f5bb7bb43c67d684d8e7b4a3bc0e376dd`，MCUboot未变、VTOR0x8200、CFSR/HFSR=0。临时+1镜像不发布。

新版 Live 真机回归74.233 s通过：连续两次全量各3026条、部分断线不删除已有ID、重连后下一次10秒采样内收到历史、实时恢复；本轮未观察到旧版60秒零历史等待。最终一致归档 integrity ok：模块3056、GPS187、无GPS手机样本78、active3321、quarantine22484，未来日期0、模块重复0。该回归支持修复有效，未证明历史现场一定由同一缺陷触发。

### 2026-10-14 生产 Release APK 真机验收

小米 Android 12/API31 已安装生产签名 App1.1.3/code5，无 DEBUGGABLE 标记。手机实际 APK SHA256 `41ee516f21465db426dbc2b84c917afeecb052fb275d2acc04c1c3832bbf23f1` 与发布 APK 完全相同，证书 SHA256 `43d4f1f5dda1a06948552fa1c5a440ff01c506d9333a0cf0b37c28e0592f794f`。

用户明确授权 Debug→生产签名切换。卸载前完整备份原 APK、原始数据库/WAL/SHM和设置；恢复19个文件逐字节匹配，一致SQLite SHA256 `ca667ad6e9eb59d45a70e3aa687efc29584ac6dc778ea23754d3d9ec364c48e6`。同生产签名临时辅助APK只用于恢复与验收，已卸载，不发布；手机迁移ZIP已删除，本机档案保留。

正式 Release 真机 Live98.168 s通过：初次连接及同步、连续两次全量各3062条、部分同步断线不删除已有ID、重连、立即增量重试及实时恢复。电压2.954→2.947 V为本轮ADC读数，未校准。最终SQLite integrity ok：active3363、模块3092、GPS187、无GPS手机样本84、quarantine22484，未来日期0、模块重复0。安装前3321条记录ID/核心字段均保留，所有原手机采样和隔离数据逐行匹配；3010条模块createdAt随全量同步刷新，属于现有入库元数据行为，非采样时间或核心数据改变。

正式发布入口：[v1.0.10](https://github.com/linckr/NRF52xxx-FieldTemp/releases/tag/v1.0.10)。验收未修改固件或正式APK字节。生产OTA与本轮Release运行均已有真实设备证据；Android13+权限、长期运行、INIT22实际失败根因、ADC校准及NOR/NVMC忙脉冲断电仍没有新增验证。
