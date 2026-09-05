# SHT40 温湿度传感器使用指南

## 概述

SHT40 是 Sensirion 公司推出的新一代数字温湿度传感器，具有高精度、低功耗、小尺寸等特点，是 SHT3x 系列的升级版本。

### 主要特性

- I2C 接口，地址 0x44 (ADDR 接地) 或 0x45 (ADDR 接 VDD)
- 温度精度: ±0.2°C (高精度模式)
- 湿度精度: ±1.8%RH (高精度模式)
- 测量时间: 1.7ms ~ 8.2ms (可配置)
- 内置加热器，可去除传感器表面冷凝
- CRC-8 数据校验
- 工作电压: 1.08V ~ 3.6V
- 超低功耗: 典型 0.4μA (待机)

### 与 AHT30 对比

| 特性 | SHT40 | AHT30 |
|------|-------|-------|
| 温度精度 | ±0.2°C | ±0.3°C |
| 湿度精度 | ±1.8%RH | ±2%RH |
| 测量时间 | 1.7~8.2ms | 80ms |
| 功耗 | 更低 | 较高 |
| 加热器 | 有 | 无 |
| CRC 校验 | 有 | 无 |
| 价格 | 较高 | 较低 |

## 硬件连接

SHT40 通过 I2C0 总线连接到 nRF52832：

| SHT40 引脚 | nRF52832 引脚 | 说明 |
|-----------|--------------|------|
| VDD | 3.3V | 电源 (1.08V ~ 3.6V) |
| GND | GND | 地 |
| SCL | P0.6 | I2C 时钟线 |
| SDA | P0.7 | I2C 数据线 |
| ADDR | GND 或 VDD | 地址选择 (GND=0x44, VDD=0x45) |

注意：
- SDA/SCL 需要 4.7kΩ 上拉电阻
- ADDR 引脚决定 I2C 地址，通常接 GND (0x44)

## 软件集成

### 1. 文件结构

```
src/sensors/
├── sht40.h              # 驱动头文件
├── sht40.c              # 驱动实现
└── sht40_example.c      # 使用示例
```

### 2. 在 CMakeLists.txt 中添加源文件

```cmake
target_sources(app PRIVATE
    src/sensors/sht40.c
    # 如果需要示例代码
    # src/sensors/sht40_example.c
)
```

### 3. 在 prj.conf 中启用 I2C

```ini
# I2C 配置
CONFIG_I2C=y
```

### 4. 在 overlay 文件中配置 I2C

确保 `app.overlay` 或对应的板级 overlay 文件中已配置 I2C0：

```dts
&i2c0 {
    compatible = "nordic,nrf-twi";
    status = "okay";
    clock-frequency = <I2C_BITRATE_STANDARD>; /* 100kHz */
    
    pinctrl-0 = <&i2c0_default>;
    pinctrl-1 = <&i2c0_sleep>;
    pinctrl-names = "default", "sleep";
};
```

## API 使用说明

### 初始化

```c
#include "sht40.h"

const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));

// 使用默认地址 0x44 (ADDR 接地)
int ret = sht40_init(i2c_dev, SHT40_I2C_ADDR_A);
if (ret != 0) {
    printk("SHT40 初始化失败: %d\n", ret);
}

// 或使用地址 0x45 (ADDR 接 VDD)
// int ret = sht40_init(i2c_dev, SHT40_I2C_ADDR_B);
```

### 读取温湿度数据

```c
int32_t temperature, humidity;

// 高精度模式 (±0.2°C, ±1.8%RH, 8.2ms)
int ret = sht40_read(&temperature, &humidity, SHT40_PRECISION_HIGH);
if (ret == 0) {
    printk("温度: %d.%02d °C\n", temperature / 100, abs(temperature % 100));
    printk("湿度: %d.%02d %%RH\n", humidity / 100, humidity % 100);
}
```

### 精度模式选择

```c
// 高精度模式 (±0.2°C, ±1.8%RH, 8.2ms) - 推荐用于精确测量
sht40_read(&temp, &humi, SHT40_PRECISION_HIGH);

// 中精度模式 (±0.3°C, ±2.5%RH, 4.5ms) - 平衡精度和速度
sht40_read(&temp, &humi, SHT40_PRECISION_MED);

// 低精度模式 (±0.4°C, ±3.5%RH, 1.7ms) - 快速测量，低功耗
sht40_read(&temp, &humi, SHT40_PRECISION_LOW);
```

### 使用加热器功能

加热器可以去除传感器表面的冷凝水，提高测量准确性。适用于高湿度环境或传感器表面有水汽时。

```c
int32_t temperature, humidity;

// 使用 200mW 加热器，持续 1 秒
int ret = sht40_read_with_heater(&temperature, &humidity, 
                                 SHT40_HEATER_200MW_1S);
if (ret == 0) {
    printk("加热后温度: %d.%02d °C\n", temperature / 100, abs(temperature % 100));
    printk("加热后湿度: %d.%02d %%RH\n", humidity / 100, humidity % 100);
}
```

加热器选项：
- `SHT40_HEATER_200MW_1S`: 200mW, 1 秒 (强力去湿)
- `SHT40_HEATER_200MW_0_1S`: 200mW, 0.1 秒 (快速去湿)
- `SHT40_HEATER_110MW_1S`: 110mW, 1 秒 (中等去湿)
- `SHT40_HEATER_110MW_0_1S`: 110mW, 0.1 秒
- `SHT40_HEATER_20MW_1S`: 20mW, 1 秒 (轻度去湿)
- `SHT40_HEATER_20MW_0_1S`: 20mW, 0.1 秒

注意：频繁使用加热器会缩短传感器寿命，建议仅在必要时使用。

### 读取序列号

每个 SHT40 都有唯一的 32 位序列号，可用于设备识别和追溯。

```c
uint32_t serial;
int ret = sht40_read_serial(&serial);
if (ret == 0) {
    printk("传感器序列号: 0x%08X\n", serial);
}
```

### 软复位

```c
// 软复位传感器（恢复到默认状态）
sht40_soft_reset();
```

## 完整示例

```c
#include "sht40.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>

void main(void)
{
    const struct device *i2c_dev;
    int32_t temperature, humidity;
    uint32_t serial;
    int ret;

    // 获取 I2C 设备
    i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
    if (!device_is_ready(i2c_dev)) {
        printk("I2C 设备未就绪\n");
        return;
    }

    // 初始化 SHT40
    ret = sht40_init(i2c_dev, SHT40_I2C_ADDR_A);
    if (ret != 0) {
        printk("初始化失败\n");
        return;
    }

    // 读取序列号
    ret = sht40_read_serial(&serial);
    if (ret == 0) {
        printk("序列号: 0x%08X\n", serial);
    }

    // 循环读取温湿度
    while (1) {
        ret = sht40_read(&temperature, &humidity, SHT40_PRECISION_HIGH);
        if (ret == 0) {
            printk("温度: %d.%02d °C, 湿度: %d.%02d %%RH\n",
                   temperature / 100, abs(temperature % 100),
                   humidity / 100, humidity % 100);
        }

        k_msleep(2000);  // 2 秒间隔
    }
}
```

## 应用场景

### 1. 高精度环境监测

```c
// 使用高精度模式，适合实验室、医疗等场景
sht40_read(&temp, &humi, SHT40_PRECISION_HIGH);
```

### 2. 低功耗应用

```c
// 使用低精度模式 + 长采样间隔
sht40_read(&temp, &humi, SHT40_PRECISION_LOW);
k_msleep(60000);  // 60 秒采样一次
```

### 3. 高湿度环境

```c
// 定期使用加热器去除冷凝
if (humidity > 9000) {  // 湿度 > 90%
    sht40_read_with_heater(&temp, &humi, SHT40_HEATER_200MW_0_1S);
} else {
    sht40_read(&temp, &humi, SHT40_PRECISION_HIGH);
}
```

### 4. 快速响应

```c
// 使用低精度模式，1.7ms 快速测量
while (1) {
    sht40_read(&temp, &humi, SHT40_PRECISION_LOW);
    k_msleep(100);  // 100ms 采样间隔
}
```

## 功耗优化

### 典型功耗

- 待机: 0.4μA
- 测量 (高精度): 平均 0.8μA @ 1 次/秒
- 测量 (低精度): 平均 0.3μA @ 1 次/秒
- 加热器 (200mW): 约 60mA @ 3.3V

### 优化建议

1. 使用低精度模式
2. 延长采样间隔
3. 避免频繁使用加热器
4. 测量完成后传感器自动进入待机模式

```c
// 低功耗配置示例
void low_power_sampling(void)
{
    int32_t temp, humi;
    
    // 每 60 秒采样一次，使用低精度模式
    while (1) {
        sht40_read(&temp, &humi, SHT40_PRECISION_LOW);
        // 处理数据...
        k_msleep(60000);  // 60 秒
    }
}
```

## 注意事项

### 1. CRC 校验

SHT40 驱动自动进行 CRC-8 校验，确保数据完整性。如果校验失败，`sht40_read()` 会返回 `-EIO` 错误。

### 2. 测量时间

不同精度模式的测量时间：
- 高精度: 8.2ms
- 中精度: 4.5ms
- 低精度: 1.7ms

驱动会自动等待测量完成，无需手动延时。

### 3. 加热器使用

- 加热器会提高温度读数（约 +2~5°C）
- 加热器会降低湿度读数
- 建议仅在高湿度环境下使用
- 频繁使用会缩短传感器寿命

### 4. I2C 地址

- 默认地址 0x44 (ADDR 接地)
- 备用地址 0x45 (ADDR 接 VDD)
- 同一总线上可连接两个 SHT40

### 5. 温度范围

- 工作范围: -40°C ~ +125°C
- 精度保证范围: 0°C ~ +65°C
- 超出范围精度会下降

### 6. 湿度范围

- 测量范围: 0% ~ 100% RH
- 精度保证范围: 10% ~ 90% RH
- 驱动会自动限制输出在 0~100% 范围内

## 故障排查

### 传感器无响应

1. 检查 I2C 连接和地址
2. 确认电源供电正常 (1.08V ~ 3.6V)
3. 检查上拉电阻 (4.7kΩ)
4. 尝试软复位: `sht40_soft_reset()`

### CRC 校验失败

1. 检查 I2C 信号质量
2. 降低 I2C 时钟频率 (100kHz)
3. 检查电源稳定性
4. 缩短 I2C 线缆长度

### 读数异常

1. 等待传感器稳定 (上电后 1ms)
2. 检查环境温湿度是否在正常范围
3. 尝试使用加热器去除冷凝
4. 软复位后重新测量

### 湿度读数偏高

1. 传感器表面可能有冷凝水
2. 使用加热器功能: `sht40_read_with_heater()`
3. 等待传感器干燥后再测量

## 与 AHT30 迁移

如果你的项目使用 AHT30，迁移到 SHT40 非常简单：

```c
// AHT30 代码
aht30_init(i2c_dev);
aht30_read(&temp, &humi);

// 迁移到 SHT40
sht40_init(i2c_dev, SHT40_I2C_ADDR_A);
sht40_read(&temp, &humi, SHT40_PRECISION_HIGH);
```

主要区别：
1. SHT40 需要指定 I2C 地址
2. SHT40 需要选择精度模式
3. SHT40 测量速度更快 (8.2ms vs 80ms)
4. SHT40 有 CRC 校验，更可靠

## 参考资料

- [SHT40 数据手册](https://sensirion.com/media/documents/33FD6951/624C4357/Datasheet_SHT4x.pdf)
- [应用笔记: 处理和组装指南](https://sensirion.com/media/documents/2B6FC1F3/6166E1F1/Sensirion_Handling_and_Assembly_Instructions_SHT4x.pdf)

## 版本历史

- v1.0 (2026-02-14): 初始版本
  - 支持三种精度模式
  - 支持加热器功能
  - CRC-8 数据校验
  - 序列号读取
