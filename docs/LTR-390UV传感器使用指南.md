# LTR-390UV 紫外线/环境光传感器使用指南

## 概述

LTR-390UV 是一款集成了紫外线 (UV) 和环境光 (ALS) 传感器的 I2C 接口芯片，适用于可穿戴设备、智能家居等应用场景。

### 主要特性

- I2C 接口，地址 0x53
- 支持 UV 和 ALS 两种测量模式
- 20 位 ADC 分辨率
- 可配置增益 (1x ~ 18x)
- 可配置积分时间 (12.5ms ~ 400ms)
- 低功耗设计
- 工作电压: 1.7V ~ 3.6V

## 硬件连接

LTR-390UV 通过 I2C0 总线连接到 nRF52832：

| LTR-390UV 引脚 | nRF52832 引脚 | 说明 |
|---------------|--------------|------|
| VDD | 3.3V | 电源 |
| GND | GND | 地 |
| SCL | P0.6 | I2C 时钟线 |
| SDA | P0.7 | I2C 数据线 |
| INT | (可选) | 中断输出 |

## 软件集成

### 1. 文件结构

```
src/sensors/
├── ltr390.h              # 驱动头文件
├── ltr390.c              # 驱动实现
└── ltr390_example.c      # 使用示例
```

### 2. 在 CMakeLists.txt 中添加源文件

```cmake
target_sources(app PRIVATE
    src/sensors/ltr390.c
    # 如果需要示例代码
    # src/sensors/ltr390_example.c
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

&pinctrl {
    i2c0_default: i2c0_default {
        group1 {
            psels = <NRF_PSEL(TWIM_SDA, 0, 7)>,
                    <NRF_PSEL(TWIM_SCL, 0, 6)>;
        };
    };

    i2c0_sleep: i2c0_sleep {
        group1 {
            psels = <NRF_PSEL(TWIM_SDA, 0, 7)>,
                    <NRF_PSEL(TWIM_SCL, 0, 6)>;
            low-power-enable;
        };
    };
};
```

## API 使用说明

### 初始化

```c
#include "ltr390.h"

const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));

int ret = ltr390_init(i2c_dev);
if (ret != 0) {
    printk("LTR-390UV 初始化失败: %d\n", ret);
}
```

### 配置传感器

#### 设置增益

```c
// 增益选项: GAIN_1, GAIN_3, GAIN_6, GAIN_9, GAIN_18
ltr390_set_gain(LTR390_GAIN_3);  // 3x 增益，适合室内
```

增益选择建议：
- 室内环境光: 18x (高灵敏度)
- 一般场景: 3x (默认)
- 户外强光: 1x (避免饱和)

#### 设置分辨率和测量速率

```c
// 分辨率选项及对应测量时间:
// RESOLUTION_20BIT_400MS  - 20位, 400ms (最高精度)
// RESOLUTION_19BIT_200MS  - 19位, 200ms
// RESOLUTION_18BIT_100MS  - 18位, 100ms (默认，平衡)
// RESOLUTION_17BIT_50MS   - 17位, 50ms
// RESOLUTION_16BIT_25MS   - 16位, 25ms
// RESOLUTION_13BIT_12_5MS - 13位, 12.5ms (最快速度)

ltr390_set_resolution(LTR390_RESOLUTION_18BIT_100MS);
```

### 读取紫外线数据

```c
uint32_t uvs_data;
int ret = ltr390_read_uvs(&uvs_data);
if (ret == 0) {
    // 计算 UV 指数 (放大 100 倍)
    int32_t uvi = ltr390_calculate_uvi(uvs_data);
    printk("UV 指数: %d.%02d\n", uvi / 100, uvi % 100);
}
```

UV 指数等级：
- 0-2: 低，可以安全待在户外
- 3-5: 中等，需要防晒措施
- 6-7: 高，必须采取防晒措施
- 8-10: 很高，避免在正午外出
- 11+: 极高，尽量待在室内

### 读取环境光数据

```c
uint32_t als_data;
int ret = ltr390_read_als(&als_data);
if (ret == 0) {
    // 计算光照强度 (Lux，放大 100 倍)
    int32_t lux = ltr390_calculate_lux(als_data);
    printk("光照强度: %d.%02d Lux\n", lux / 100, lux % 100);
}
```

光照强度参考：
- 0.1 Lux: 月光
- 10-50 Lux: 室内照明
- 100-500 Lux: 办公室照明
- 1000 Lux: 阴天室外
- 10000-25000 Lux: 晴天室外
- 100000 Lux: 直射阳光

## 完整示例

```c
#include "ltr390.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>

void main(void)
{
    const struct device *i2c_dev;
    uint32_t uvs_data, als_data;
    int32_t uvi, lux;
    int ret;

    // 获取 I2C 设备
    i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
    if (!device_is_ready(i2c_dev)) {
        printk("I2C 设备未就绪\n");
        return;
    }

    // 初始化传感器
    ret = ltr390_init(i2c_dev);
    if (ret != 0) {
        printk("初始化失败\n");
        return;
    }

    // 配置传感器
    ltr390_set_gain(LTR390_GAIN_3);
    ltr390_set_resolution(LTR390_RESOLUTION_18BIT_100MS);

    // 循环读取
    while (1) {
        // 读取 UV
        ret = ltr390_read_uvs(&uvs_data);
        if (ret == 0) {
            uvi = ltr390_calculate_uvi(uvs_data);
            printk("UV 指数: %d.%02d\n", uvi / 100, uvi % 100);
        }

        // 读取环境光
        ret = ltr390_read_als(&als_data);
        if (ret == 0) {
            lux = ltr390_calculate_lux(als_data);
            printk("光照: %d.%02d Lux\n", lux / 100, lux % 100);
        }

        k_msleep(5000);  // 5 秒间隔
    }
}
```

## 场景配置建议

### 室内环境光监测

```c
ltr390_set_gain(LTR390_GAIN_18);              // 高灵敏度
ltr390_set_resolution(LTR390_RESOLUTION_20BIT_400MS);  // 高精度
```

### 户外 UV 监测

```c
ltr390_set_gain(LTR390_GAIN_1);               // 避免饱和
ltr390_set_resolution(LTR390_RESOLUTION_16BIT_25MS);   // 快速响应
```

### 低功耗模式

```c
ltr390_set_gain(LTR390_GAIN_3);               // 平衡
ltr390_set_resolution(LTR390_RESOLUTION_13BIT_12_5MS); // 最快速度
```

## 注意事项

1. **模式切换**: LTR-390UV 不能同时测量 UV 和 ALS，需要切换模式。驱动会自动处理模式切换，但每次切换需要等待一个测量周期。

2. **数据就绪**: 读取数据前，驱动会自动等待数据就绪。根据配置的分辨率，等待时间从 12.5ms 到 400ms 不等。

3. **增益选择**: 
   - 增益过高可能导致强光下饱和
   - 增益过低可能导致弱光下精度不足
   - 建议根据实际环境动态调整

4. **UV 测量**: 
   - 玻璃会阻挡大部分 UV，室内测量值通常很低
   - 建议在户外或窗边测量
   - 阴天也有 UV 辐射

5. **功耗优化**:
   - 使用较短的积分时间可降低功耗
   - 不需要时可以关闭传感器 (写 0x00 到 MAIN_CTRL)

## 故障排查

### 传感器无响应

1. 检查 I2C 连接和地址 (0x53)
2. 确认电源供电正常 (1.7V ~ 3.6V)
3. 检查上拉电阻 (SCL/SDA 需要 4.7kΩ 上拉)

### 读数异常

1. 检查增益设置是否合适
2. 确认测量环境 (室内 UV 通常很低)
3. 等待足够的测量时间

### 数据饱和

1. 降低增益 (从 18x 降到 1x)
2. 缩短积分时间
3. 避免传感器直接对准强光源

## 参考资料

- [LTR-390UV 数据手册](https://optoelectronics.liteon.com/upload/download/DS86-2015-0004/LTR-390UV_Final_%20DS_V1%201.pdf)
- [应用笔记: UV 指数计算](https://optoelectronics.liteon.com/upload/download/AN86-2015-0003/AN_LTR-390UV_UV%20Sensor_ver%201.pdf)

## 版本历史

- v1.0 (2026-02-14): 初始版本
  - 支持 UV 和 ALS 测量
  - 可配置增益和分辨率
  - UV 指数和 Lux 计算
