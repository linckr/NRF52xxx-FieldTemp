# nRF52832 SPI Flash (W25Q64) 访问问题调试指南

## 问题描述
nRF52832 无法访问 SPI Flash 存储器 (W25Q64)

## 已知配置
- **芯片**: nRF52832
- **Flash**: W25Q64 (8MB SPI NOR Flash)
- **SPI 引脚**: SCK=P0.30, MOSI=P0.31, MISO=P0.29, CS=P0.28
- **使用外设**: SPI1 (因为 SPI0 与 I2C0 共用基址 0x40003000)

## 可能原因及解决方案

### 1. SPI1 设备树节点未启用

**检查方法**:
编译后查看生成的设备树:
```bash
west build -b nrf52832dk_nrf52832 --build-dir build_nrf52832
cat build_nrf52832/zephyr/zephyr.dts | grep -A 20 "spi1"
```

**解决方案**: 已在 `boards/nrf52832dk_nrf52832_cpuapp.overlay` 中添加完整的 SPI1 配置

### 2. SPI 频率过高导致通信不稳定

**原因**: 
- 硬件布线较长或有干扰
- 电源不稳定
- Flash 芯片质量问题

**解决方案**: 已将 SPI 频率从 8MHz 降低到 4MHz

### 3. CS 引脚配置问题

**检查**:
- CS 引脚 (P0.28) 是否正确配置为 GPIO 输出
- CS 引脚是否与其他功能冲突

**验证代码**:
```c
const struct device *gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
if (!device_is_ready(gpio_dev)) {
    printk("GPIO设备未就绪\n");
}
int ret = gpio_pin_configure(gpio_dev, 28, GPIO_OUTPUT_INACTIVE);
printk("CS引脚配置结果: %d\n", ret);
```

### 4. 引脚复用冲突

**检查项目**:
- P0.28 是否被其他外设使用 (UART, I2C, 其他 SPI)
- P0.30/31/29 是否有冲突

**查看方法**:
```bash
grep -r "P0.28\|P0.30\|P0.31\|P0.29" boards/nrf52832dk_nrf52832_cpuapp.overlay
```

### 5. 电源问题

**检查**:
- Flash 芯片供电是否稳定 (3.3V)
- 是否有足够的去耦电容
- 电源纹波是否过大

**测试**: 使用万用表测量 VCC 引脚电压

### 6. 硬件连接问题

**检查清单**:
- [ ] SCK, MOSI, MISO, CS 引脚是否正确连接
- [ ] 是否有虚焊或接触不良
- [ ] 信号线是否过长 (建议 < 10cm)
- [ ] 是否有上拉/下拉电阻配置错误

### 7. Flash 芯片损坏或型号不匹配

**验证方法**:
读取 JEDEC ID 应该返回: `0xEF 0x40 0x17`

**测试代码** (已在 w25q64_init 中):
```c
uint8_t id[3];
// ... SPI 读取 JEDEC ID ...
printk("JEDEC ID: 0x%02X 0x%02X 0x%02X\n", id[0], id[1], id[2]);
```

## 调试步骤

### 步骤 1: 编译并烧录固件
```bash
west build -b nrf52832dk_nrf52832 --build-dir build_nrf52832
west flash -d build_nrf52832
```

### 步骤 2: 查看串口日志
连接串口 (115200 8N1)，查看以下关键信息:
```
[W25Q64] 使用 SPI1 (nRF52832)
[W25Q64] SPI设备名称: spi@40004000
[W25Q64] SPI设备已就绪
[W25Q64] CS GPIO配置成功
[W25Q64] JEDEC ID: 0xEF 0x40 0x17
```

### 步骤 3: 如果 "SPI设备未就绪"

检查设备树配置:
```bash
# 查看编译后的设备树
cat build_nrf52832/zephyr/zephyr.dts | grep -A 30 "spi@40004000"

# 查看 Kconfig 配置
grep SPI build_nrf52832/zephyr/.config
```

确认以下配置已启用:
```
CONFIG_SPI=y
CONFIG_SPI_NRFX=y
CONFIG_SPI_1=y  # 或 CONFIG_SPI_NRFX_SPIM1=y
```

### 步骤 4: 如果 JEDEC ID 读取失败或返回全 0xFF

可能原因:
1. **MISO 引脚无信号** - 检查硬件连接
2. **Flash 未上电** - 检查电源
3. **SPI 时序问题** - 降低频率到 1MHz 测试

临时测试代码 (降低到 1MHz):
```c
w25q64_spi_cfg.frequency = 1000000;  // 1MHz
```

### 步骤 5: 如果 JEDEC ID 正确但读写失败

检查:
1. 块保护位是否启用 - 查看日志中的 BP 值
2. 写使能是否成功 - WEL 位应该为 1
3. 是否有超时 - 增加超时时间

## 硬件测试方法

### 使用逻辑分析仪
捕获 SPI 信号，检查:
- SCK 频率是否正确
- MOSI 数据是否正确发送
- MISO 是否有响应
- CS 是否正确拉低/拉高

### 使用示波器
测量:
- SCK 信号质量 (上升/下降沿)
- 信号完整性 (是否有振铃、过冲)
- 电源纹波

## 常见错误代码

| 错误码 | 含义 | 可能原因 |
|--------|------|----------|
| -ENODEV (-19) | 设备不存在 | SPI1 未在设备树中启用 |
| -ETIMEDOUT (-110) | 超时 | Flash 未响应，硬件连接问题 |
| -EIO (-5) | I/O 错误 | SPI 通信失败 |
| -EINVAL (-22) | 参数无效 | 地址或长度错误 |

## 最小测试代码

如果以上都无法解决，可以使用以下最小测试代码:

```c
void test_spi_flash(void)
{
    const struct device *spi = DEVICE_DT_GET(DT_NODELABEL(spi1));
    if (!device_is_ready(spi)) {
        printk("SPI1 未就绪\n");
        return;
    }
    
    // 配置 CS 引脚
    const struct device *gpio = DEVICE_DT_GET(DT_NODELABEL(gpio0));
    gpio_pin_configure(gpio, 28, GPIO_OUTPUT_INACTIVE);
    
    // 读取 JEDEC ID
    struct spi_config cfg = {
        .frequency = 1000000,  // 1MHz
        .operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_TRANSFER_MSB,
    };
    
    uint8_t tx[4] = {0x9F, 0xFF, 0xFF, 0xFF};
    uint8_t rx[4] = {0};
    
    struct spi_buf tx_buf = {.buf = tx, .len = 4};
    struct spi_buf rx_buf = {.buf = rx, .len = 4};
    struct spi_buf_set tx_set = {.buffers = &tx_buf, .count = 1};
    struct spi_buf_set rx_set = {.buffers = &rx_buf, .count = 1};
    
    gpio_pin_set(gpio, 28, 0);  // CS 拉低
    int ret = spi_transceive(spi, &cfg, &tx_set, &rx_set);
    gpio_pin_set(gpio, 28, 1);  // CS 拉高
    
    printk("SPI 返回: %d, ID: %02X %02X %02X\n", ret, rx[1], rx[2], rx[3]);
}
```

## 修改记录

### 2024-02-14
1. 将 SPI 频率从 8MHz 降低到 4MHz
2. 在 overlay 中添加 W25Q64 设备节点
3. 增加详细的调试日志输出
4. 添加 SPI 设备名称和状态检查

## 下一步

如果问题仍未解决，请提供:
1. 完整的串口日志输出
2. 硬件原理图 (SPI 部分)
3. 使用的 Flash 芯片型号和批次
4. 逻辑分析仪捕获的 SPI 波形 (如果有)
