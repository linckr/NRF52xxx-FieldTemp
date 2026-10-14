# 生产 Release APK 设备验收

### 2026-10-14 生产 Release APK 真机验收

小米 Android 12/API31 已安装生产签名 App1.1.3/code5，无 DEBUGGABLE 标记。手机实际 APK SHA256 `41ee516f21465db426dbc2b84c917afeecb052fb275d2acc04c1c3832bbf23f1` 与发布 APK 完全相同，证书 SHA256 `43d4f1f5dda1a06948552fa1c5a440ff01c506d9333a0cf0b37c28e0592f794f`。

用户明确授权 Debug→生产签名切换。卸载前完整备份原 APK、原始数据库/WAL/SHM和设置；恢复19个文件逐字节匹配，一致SQLite SHA256 `ca667ad6e9eb59d45a70e3aa687efc29584ac6dc778ea23754d3d9ec364c48e6`。同生产签名临时辅助APK只用于恢复与验收，已卸载，不发布；手机迁移ZIP已删除，本机档案保留。

正式 Release 真机 Live98.168 s通过：初次连接及同步、连续两次全量各3062条、部分同步断线不删除已有ID、重连、立即增量重试及实时恢复。电压2.954→2.947 V为本轮ADC读数，未校准。最终SQLite integrity ok：active3363、模块3092、GPS187、无GPS手机样本84、quarantine22484，未来日期0、模块重复0。安装前3321条记录ID/核心字段均保留，所有原手机采样和隔离数据逐行匹配；3010条模块createdAt随全量同步刷新，属于现有入库元数据行为，非采样时间或核心数据改变。

正式发布入口：[v1.0.10](https://github.com/linckr/NRF52xxx-FieldTemp/releases/tag/v1.0.10)。验收未修改固件或正式APK字节。生产OTA与本轮Release运行均已有真实设备证据；Android13+权限、长期运行、INIT22实际失败根因、ADC校准及NOR/NVMC忙脉冲断电仍没有新增验证。
