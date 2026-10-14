# 上电提示与三位电压最终验收

## 2026-10-14 最新验收：上电提示与三位电压

当前 Firmware **1.0.11+0**、生产 App **1.1.4/code6**、Room11。LINK/P0.5为实物唯一已确认可见的灯：应用启动最早阶段250 ms亮/250 ms灭，6次共3秒，再启动蓝牙。用户已确认整根SWD断开后的电池冷启动“闪烁、熄灭”，以及“连接常亮、断开熄灭”。启动软件重启也提示；MCUboot在进入应用之前失败时仍无提示，不代表电池座接触缺陷已修复。

App首页、配置、历史列表/详情、设备管理、电池指示及日志7处电压显示统一3位小数；毫伏存储/协议、Room版本和ADC测量精度未改。最终生产APK实际安装读回SHA256 `488bc447c9bc602abf4f005058a367fb4228b1a82a621f61b2fb2c41a991958c`，证书与原生产证书一致。首次DATA/P0.4版本肉眼未通过，用户确认只有一颗可见灯后改用LINK；未更改引脚定义。

最终签名主槽152362 B完整回读SHA256 `47760eb524598918ce5aefaf10060b65c2ccfe57e70b1c282bbf692f5d18bd43`。只更新primary，MCUboot、UICR不变，外部NVS/history未编程；VTOR0x8200、CFSR/HFSR0。App Flash151700 B（较1.0.10 +16）、RAM23016 B（-64，静态余1560）、MCUboot31876 B（余892，构建和设备字节均未变）。容量/版本门禁、生产公钥验签、真实C历史fixture、123 JVM及Release lint通过。

最终配对Live73.935 s通过：两次全量各3154条、部分断线保留、重连、增量重试及实时恢复。最新一致归档integrity ok：active3548、模块3184、GPS187、无GPS手机样本177；原3477条ID/核心字段、所有原手机采样与隔离记录保留，未来日期/模块重复0。回归中的反复全量是明确opt-in测试行为；测试已结束、辅助APK已卸载，App恢复正常增量同步，临时USB亮屏设置恢复0。

已知边界：当前归档有7条temperature/humidity均0的手机来源采样，模块来源无此类行；保留原数据，手机采样有效性应单独排查。首次测试发生BLUETOOTH_CONNECT权限异常，重新授权后回归通过，未声称缺失/撤回权限路径已修复。INIT22根因、ADC校准、Android13+权限与长期运行、NOR/NVMC忙脉冲断电仍待验证。本版本通过SWD部署及冷启动验证；生产OTA搬运证明来自1.0.10历史批次，本次没有重新做OTA搬运，不得混同。

正式发布：[v1.0.11](https://github.com/linckr/NRF52xxx-FieldTemp/releases/tag/v1.0.11)。完整证据见 [上电与三位电压验收](https://github.com/linckr/NRF52xxx-FieldTemp/blob/main/docs/20261014-startup-voltage-validation.md)。以下旧版本状态按历史记录理解，以本节和当前代码为准。
