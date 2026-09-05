# PM 设备管理问题说明

## 问题描述

在编译项目时遇到链接错误：

```
undefined reference to `__device_dts_ord_77'
```

## 问题原因

1. **设备树配置**：设备树中某些设备（如 `i2c0`、`adc`）带有 `zephyr,pm-device-runtime-auto` 属性
2. **PM 设备功能**：启用 `CONFIG_PM_DEVICE=y` 后，系统会为这些设备自动创建 PM（电源管理）设备实例
3. **驱动实现问题**：某些设备的驱动可能未正确实现 PM 功能，导致链接时找不到对应的设备符号 `__device_dts_ord_77`

## 当前解决方案

### 方案 1：禁用 PM_DEVICE（已采用）

在 `prj.conf` 中禁用 `CONFIG_PM_DEVICE`：

```conf
# 电源管理（低功耗必需）
CONFIG_PM=y
# 暂时禁用 PM_DEVICE，避免链接错误 __device_dts_ord_77
# CONFIG_PM_DEVICE=y
```

**影响**：

- ✅ 系统级电源管理（`k_sleep()` 等）仍然可用
- ✅ 系统可以正常进入低功耗模式
- ❌ 设备级自动 PM 管理被禁用（设备不会自动进入低功耗状态）

### 方案 2：在设备树中删除 PM 运行时属性（备选）

在 `app.overlay` 中删除设备的 `zephyr,pm-device-runtime-auto` 属性：

```dts
&i2c0 {
    /delete-property/ zephyr,pm-device-runtime-auto;
};

&adc {
    /delete-property/ zephyr,pm-device-runtime-auto;
};
```

**注意**：此方案需要确保 `CONFIG_PM_DEVICE_RUNTIME=n`，否则可能仍有问题。

## 功能影响

### 当前状态（CONFIG_PM_DEVICE 禁用）

- ✅ **系统级低功耗**：`k_sleep()` 可以让系统进入 System ON 模式，功耗降低
- ✅ **定时器唤醒**：系统可以从低功耗模式被定时器中断唤醒
- ✅ **蓝牙唤醒**：系统可以从低功耗模式被蓝牙中断唤醒
- ❌ **设备级自动 PM**：I2C、ADC 等设备不会自动进入低功耗状态（需要手动控制）

### 如果启用 CONFIG_PM_DEVICE

- ✅ **设备级自动 PM**：设备在空闲时自动进入低功耗状态
- ✅ **更精细的功耗控制**：可以单独控制每个设备的电源状态
- ❌ **当前问题**：链接错误 `__device_dts_ord_77` 未定义

## 未来解决方案

### 方案 A：等待驱动修复

等待 Zephyr/NCS 更新，修复相关驱动的 PM 设备实现问题。

### 方案 B：手动实现设备 PM 控制

如果需要设备级 PM 管理，可以：

1. 在代码中手动控制设备的电源状态
2. 使用 HAL 层 API 直接操作寄存器
3. 不使用 Zephyr 的 PM_DEVICE 框架

### 方案 C：使用设备树覆盖

创建专门的设备树覆盖文件，确保所有设备的 PM 配置正确。

## 相关文件

- `prj.conf`：电源管理配置
- `app.overlay`：设备树覆盖配置
- `src/main.c`：主程序（已注释 PM_DEVICE 相关头文件）

## 参考信息

- Zephyr PM 设备文档：https://docs.zephyrproject.org/latest/services/pm/device.html
- NCS 3.2.1 电源管理：https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/zephyr/services/pm/device.html
- 设备树 PM 属性：`zephyr,pm-device-runtime-auto`

## 更新记录

- 2024-XX-XX：首次记录问题，采用方案 1（禁用 CONFIG_PM_DEVICE）
