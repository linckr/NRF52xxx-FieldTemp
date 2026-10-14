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
