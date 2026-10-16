# 项目交接

当前开发入口和任务状态统一见[CODEX_START_HERE](CODEX_START_HERE.md)，避免多份“当前状态”漂移。

## 当前基线

开发候选 Firmware1.0.12+0、App1.1.5/code7、Room12；正式发布与设备固件仍为1.0.11。新固件尚未真机验收；主仓库`D:\下载\PandaTemperature`，App在`android/`。origin为linckr/NRF52xxx-FieldTemp，upstream为yodfz/NRF52xxx-FieldTemp。HEAD及PR是否合并必须实时查询，不把历史PR状态当当前结论。

正式1.0.11签名镜像SHA256：`47760eb524598918ce5aefaf10060b65c2ccfe57e70b1c282bbf692f5d18bd43`；App1.1.4生产APK SHA256：`488bc447c9bc602abf4f005058a367fb4228b1a82a621f61b2fb2c41a991958c`。

## 已验证与限制

- 当前板唯一已确认可见灯为LINK/P0.5：电池冷启动闪烁三秒后灭，连接常亮、断开熄灭。DATA/P0.4仍有代码，不能据此声称实物有第二颗灯。
- 小米 Android12 当前已安装生产签名 App1.1.5/code7、Room12，最终 APK 回读一致，原3935行及设置保留，11+1项本地历史/权限回归通过，见[10月16日验证](docs/20261016-可靠性修复验证.md)。旧1.1.4配对Live73.935秒等属于[10月14日历史验收](docs/20261014-startup-voltage-validation.md)，不能套用为新版本配对耗时。
- 生产MCUboot信任已部署，1.0.10生产签名测试镜像OTA搬运及恢复正式镜像均完整回读验证；1.0.11不重复声称有新OTA搬运实证。
- 已验收1.0.11：Flash151700 B、RAM23016 B、MCUboot31876 B；候选1.0.12：Flash152508 B、RAM23016 B、MCUboot31876 B，引导二进制逐字一致；所有分区未改。有效历史容量64848，预留完整扇区回收。
- 10月15日发现冷启动离线绝对时间不可信的使用限制，realme事发时使用旧App1.1.0/Room10，随后已覆盖升级生产1.1.4并验证原3939行完整保留；调查和恢复不能套用小米验收，不能无证据平移或覆盖原记录。
- INIT22尚未抓到实际失败根因，ADC未校准，Android12离线/权限撤回回归已通过，Android13+及长期测试未完成，NOR/NVMC忙脉冲断电未覆盖。候选已移除Release的Debug回退，并验证缺配置和误用Debug证书均失败。

## 代码地图

| 路径 | 用途 |
|---|---|
| src/main.c | 采样、聚合、时间基准、GATT、历史环与传输调度 |
| src/ble/ble_ota.c | 自定义OTA上传、续传、END校验及安装触发 |
| src/ble/ble_adv.c | 广播名称和快/慢/间歇策略 |
| src/storage/ | Zephyr SPI NOR访问、NVS配置和历史编码 |
| src/sensors/aht30.c、spl06.c | 当前温湿度和气压来源 |
| src/led.c、src/vdd.c | 指示灯和内部VDD测量 |
| android/ | 唯一日常Android开发源；独立仓库受控同步上游 |
| pm_static.yml、app.overlay、sysbuild/ | 分区、设备树、MCUboot子镜像配置；修改须明确授权 |
| tools/ | 当前构建、空间/版本门禁、诊断工具 |
| docs/history/、docs/archive/ | 历史实证/过时设计及仅供阅读的工具，非发布入口 |

迁移前详细过程、旧代码地图和历史任务见[整理前快照](docs/history/20261015-HANDOFF.md)。它包含已取消策略和旧版本，不直接执行其中旧烧录命令。

本轮开发及验证入口：[离线时间机制](docs/20261016-离线时间机制.md)、[断电和诊断边界](docs/20261016-OTA精确断电与诊断待验收.md)。
