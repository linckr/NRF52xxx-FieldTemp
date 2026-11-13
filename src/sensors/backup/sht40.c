/**
 * @file sht40.c
 * @brief SHT40 温湿度传感器驱动
 */

#include "sht40.h"
#include <zephyr/sys/printk.h>

/* 模块内静态变量 */
static const struct device *i2c_dev_internal;
static uint8_t i2c_addr_internal = SHT40_I2C_ADDR_A;

/* 获取 I2C 设备指针 */
const struct device *sht40_get_i2c_dev(void)
{
	return i2c_dev_internal;
}

/* 获取 I2C 地址 */
uint8_t sht40_get_i2c_addr(void)
{
	return i2c_addr_internal;
}

/* CRC-8 校验 (多项式: 0x31, 初始值: 0xFF) */
static uint8_t sht40_crc8(const uint8_t *data, size_t len)
{
	uint8_t crc = 0xFF;
	
	for (size_t i = 0; i < len; i++) {
		crc ^= data[i];
		for (uint8_t bit = 0; bit < 8; bit++) {
			if (crc & 0x80) {
				crc = (crc << 1) ^ 0x31;
			} else {
				crc = crc << 1;
			}
		}
	}
	
	return crc;
}

/* 发送命令 */
static int sht40_send_command(uint8_t cmd)
{
	return i2c_write(i2c_dev_internal, &cmd, 1, i2c_addr_internal);
}

/* SHT40 初始化函数 */
int sht40_init(const struct device *i2c_dev, uint8_t addr)
{
	int ret;

	/* 保存 I2C 设备指针和地址 */
	i2c_dev_internal = i2c_dev;
	i2c_addr_internal = addr;

	if (!device_is_ready(i2c_dev)) {
		printk("SHT40: I2C 设备未就绪\r\n");
		return -ENODEV;
	}
	printk("SHT40: I2C 设备已就绪 (SDA: P0.7, SCL: P0.6)\r\n");

	/* 等待传感器上电稳定 */
	k_msleep(1);

	/* 软复位传感器 */
	ret = sht40_soft_reset();
	if (ret != 0) {
		printk("SHT40 (地址 0x%02X) 无响应, 错误: %d\r\n", addr, ret);
		return ret;
	}

	/* 读取序列号验证传感器 */
	uint32_t serial;
	ret = sht40_read_serial(&serial);
	if (ret == 0) {
		printk("SHT40 已检测到 (地址: 0x%02X, 序列号: 0x%08X)\r\n", 
		       addr, serial);
	} else {
		printk("SHT40 序列号读取失败，但传感器可能正常工作\r\n");
	}

	printk("SHT40 传感器初始化完成\r\n");
	return 0;
}

/* 软复位 */
int sht40_soft_reset(void)
{
	int ret;

	/* 检查 I2C 设备 */
	if (i2c_dev_internal == NULL || !device_is_ready(i2c_dev_internal)) {
		printk("SHT40 I2C 设备未就绪\r\n");
		return -ENODEV;
	}

	/* 发送软复位命令 */
	ret = sht40_send_command(SHT40_CMD_SOFT_RESET);
	if (ret != 0) {
		printk("SHT40 软复位命令发送失败: %d\r\n", ret);
		return ret;
	}

	/* 等待复位完成 (最大 1ms) */
	k_msleep(1);

	return 0;
}

/* 读取温湿度数据 */
int sht40_read(int32_t *temperature, int32_t *humidity, sht40_precision_t precision)
{
	uint8_t cmd;
	uint8_t data[6];  /* 温度(2字节) + CRC(1字节) + 湿度(2字节) + CRC(1字节) */
	uint16_t raw_temp, raw_humi;
	uint32_t wait_ms;
	int ret;

	/* 检查 I2C 设备 */
	if (i2c_dev_internal == NULL || !device_is_ready(i2c_dev_internal)) {
		printk("SHT40 I2C 设备未就绪\r\n");
		return -ENODEV;
	}

	/* 根据精度选择命令和等待时间 */
	switch (precision) {
		case SHT40_PRECISION_HIGH:
			cmd = SHT40_CMD_MEASURE_HIGH;
			wait_ms = 10;  /* 8.2ms + 余量 */
			break;
		case SHT40_PRECISION_MED:
			cmd = SHT40_CMD_MEASURE_MED;
			wait_ms = 5;   /* 4.5ms + 余量 */
			break;
		case SHT40_PRECISION_LOW:
			cmd = SHT40_CMD_MEASURE_LOW;
			wait_ms = 2;   /* 1.7ms + 余量 */
			break;
		default:
			return -EINVAL;
	}

	/* 发送测量命令 */
	ret = sht40_send_command(cmd);
	if (ret != 0) {
		printk("SHT40 测量命令发送失败: %d\r\n", ret);
		return ret;
	}

	/* 等待测量完成 */
	k_msleep(wait_ms);

	/* 读取 6 字节数据 */
	ret = i2c_read(i2c_dev_internal, data, 6, i2c_addr_internal);
	if (ret != 0) {
		printk("SHT40 数据读取失败: %d\r\n", ret);
		return ret;
	}

	/* 验证温度 CRC */
	if (sht40_crc8(&data[0], 2) != data[2]) {
		printk("SHT40 温度 CRC 校验失败\r\n");
		return -EIO;
	}

	/* 验证湿度 CRC */
	if (sht40_crc8(&data[3], 2) != data[5]) {
		printk("SHT40 湿度 CRC 校验失败\r\n");
		return -EIO;
	}

	/* 解析温度数据 (16位) */
	raw_temp = ((uint16_t)data[0] << 8) | data[1];

	/* 解析湿度数据 (16位) */
	raw_humi = ((uint16_t)data[3] << 8) | data[4];

	/* 转换为实际值 (放大100倍，避免浮点运算)
	 * 温度公式: T = -45 + 175 * (raw / 65535)
	 *          = -4500 + 17500 * raw / 65535
	 * 湿度公式: RH = -6 + 125 * (raw / 65535)
	 *          = -600 + 12500 * raw / 65535
	 */
	*temperature = -4500 + (int32_t)((raw_temp * 17500ULL) / 65535ULL);
	*humidity = -600 + (int32_t)((raw_humi * 12500ULL) / 65535ULL);

	/* 湿度限制在 0-100% 范围内 */
	if (*humidity < 0) {
		*humidity = 0;
	} else if (*humidity > 10000) {
		*humidity = 10000;
	}

	return 0;
}

/* 使用加热器进行测量 */
int sht40_read_with_heater(int32_t *temperature, int32_t *humidity, 
                           sht40_heater_t heater)
{
	uint8_t cmd;
	uint8_t data[6];
	uint16_t raw_temp, raw_humi;
	uint32_t wait_ms;
	int ret;

	/* 检查 I2C 设备 */
	if (i2c_dev_internal == NULL || !device_is_ready(i2c_dev_internal)) {
		printk("SHT40 I2C 设备未就绪\r\n");
		return -ENODEV;
	}

	/* 根据加热器配置选择命令和等待时间 */
	switch (heater) {
		case SHT40_HEATER_OFF:
			/* 不使用加热器，使用高精度测量 */
			return sht40_read(temperature, humidity, SHT40_PRECISION_HIGH);
			
		case SHT40_HEATER_200MW_1S:
			cmd = SHT40_CMD_HEAT_200MW_1S;
			wait_ms = 1100;  /* 1000ms + 余量 */
			break;
			
		case SHT40_HEATER_200MW_0_1S:
			cmd = SHT40_CMD_HEAT_200MW_0_1S;
			wait_ms = 150;   /* 100ms + 余量 */
			break;
			
		case SHT40_HEATER_110MW_1S:
			cmd = SHT40_CMD_HEAT_110MW_1S;
			wait_ms = 1100;
			break;
			
		case SHT40_HEATER_110MW_0_1S:
			cmd = SHT40_CMD_HEAT_110MW_0_1S;
			wait_ms = 150;
			break;
			
		case SHT40_HEATER_20MW_1S:
			cmd = SHT40_CMD_HEAT_20MW_1S;
			wait_ms = 1100;
			break;
			
		case SHT40_HEATER_20MW_0_1S:
			cmd = SHT40_CMD_HEAT_20MW_0_1S;
			wait_ms = 150;
			break;
			
		default:
			return -EINVAL;
	}

	/* 发送加热器命令 */
	ret = sht40_send_command(cmd);
	if (ret != 0) {
		printk("SHT40 加热器命令发送失败: %d\r\n", ret);
		return ret;
	}

	/* 等待加热和测量完成 */
	k_msleep(wait_ms);

	/* 读取 6 字节数据 */
	ret = i2c_read(i2c_dev_internal, data, 6, i2c_addr_internal);
	if (ret != 0) {
		printk("SHT40 数据读取失败: %d\r\n", ret);
		return ret;
	}

	/* 验证 CRC */
	if (sht40_crc8(&data[0], 2) != data[2] || 
	    sht40_crc8(&data[3], 2) != data[5]) {
		printk("SHT40 CRC 校验失败\r\n");
		return -EIO;
	}

	/* 解析数据 */
	raw_temp = ((uint16_t)data[0] << 8) | data[1];
	raw_humi = ((uint16_t)data[3] << 8) | data[4];

	/* 转换为实际值 */
	*temperature = -4500 + (int32_t)((raw_temp * 17500ULL) / 65535ULL);
	*humidity = -600 + (int32_t)((raw_humi * 12500ULL) / 65535ULL);

	/* 湿度限制 */
	if (*humidity < 0) {
		*humidity = 0;
	} else if (*humidity > 10000) {
		*humidity = 10000;
	}

	return 0;
}

/* 读取序列号 */
int sht40_read_serial(uint32_t *serial)
{
	uint8_t data[6];  /* 序列号(2字节) + CRC(1字节) + 序列号(2字节) + CRC(1字节) */
	int ret;

	/* 检查 I2C 设备 */
	if (i2c_dev_internal == NULL || !device_is_ready(i2c_dev_internal)) {
		printk("SHT40 I2C 设备未就绪\r\n");
		return -ENODEV;
	}

	/* 发送读取序列号命令 */
	ret = sht40_send_command(SHT40_CMD_READ_SERIAL);
	if (ret != 0) {
		printk("SHT40 序列号命令发送失败: %d\r\n", ret);
		return ret;
	}

	/* 等待命令执行 (最大 1ms) */
	k_msleep(1);

	/* 读取 6 字节数据 */
	ret = i2c_read(i2c_dev_internal, data, 6, i2c_addr_internal);
	if (ret != 0) {
		printk("SHT40 序列号读取失败: %d\r\n", ret);
		return ret;
	}

	/* 验证 CRC */
	if (sht40_crc8(&data[0], 2) != data[2] || 
	    sht40_crc8(&data[3], 2) != data[5]) {
		printk("SHT40 序列号 CRC 校验失败\r\n");
		return -EIO;
	}

	/* 组合序列号 (32位) */
	*serial = ((uint32_t)data[0] << 24) | 
	          ((uint32_t)data[1] << 16) | 
	          ((uint32_t)data[3] << 8) | 
	          (uint32_t)data[4];

	return 0;
}
