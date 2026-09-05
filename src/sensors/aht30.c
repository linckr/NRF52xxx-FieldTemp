/**
 * @file aht30.c
 * @brief AHT30 温湿度传感器驱动
 */

#include "aht30.h"
#include <zephyr/sys/printk.h>

/* 模块内静态 I2C 设备指针 */
static const struct device *i2c_dev_internal;

/* 获取 I2C 设备指针 */
const struct device *aht30_get_i2c_dev(void)
{
	return i2c_dev_internal;
}

/* AHT30 初始化函数 */
int aht30_init(const struct device *i2c_dev)
{
	uint8_t cmd[3];
	int ret;

	/* 保存 I2C 设备指针 */
	i2c_dev_internal = i2c_dev;

	if (!device_is_ready(i2c_dev)) {
		printk("I2C 设备未就绪\r\n");
		return -ENODEV;
	}
	printk("I2C 设备已就绪 (SDA: P0.7, SCL: P0.6)\r\n");

	/* 等待传感器上电稳定 */
	k_msleep(100);

	/* 检查 AHT30 是否存在 */
	uint8_t status;
	ret = i2c_read(i2c_dev, &status, 1, AHT30_I2C_ADDR);
	if (ret != 0) {
		printk("AHT30 (地址 0x%02X) 无响应, 错误: %d\r\n", AHT30_I2C_ADDR, ret);
		return ret;
	}
	printk("AHT30 已检测到 (状态: 0x%02X)\r\n", status);

	/* 检查是否需要校准 (bit3=0 表示未校准) */
	if (!(status & 0x08)) {
		printk("AHT30 需要初始化校准...\r\n");
		/* 发送初始化命令 */
		cmd[0] = AHT30_CMD_INIT;
		cmd[1] = 0x08;
		cmd[2] = 0x00;
		ret = i2c_write(i2c_dev, cmd, 3, AHT30_I2C_ADDR);
		if (ret != 0) {
			printk("AHT30 初始化命令发送失败: %d\r\n", ret);
			return ret;
		}
		/* 等待初始化完成 */
		k_msleep(10);
	}
	
	printk("AHT30 传感器初始化完成\r\n");
	return 0;
}

/* AHT30 读取温湿度函数 */
int aht30_read(int32_t *temperature, int32_t *humidity)
{
	uint8_t cmd[3];
	uint8_t data[7];
	int ret;
	uint32_t raw_humidity;
	uint32_t raw_temperature;

	/* 检查 I2C 设备是否已初始化 */
	if (i2c_dev_internal == NULL || !device_is_ready(i2c_dev_internal)) {
		printk("AHT30 I2C 设备未就绪\r\n");
		return -ENODEV;
	}

	/* 1. 发送测量命令前，先检查状态，若忙则等待 */
	/* 这种预防性检查可以减少“命令发送后传感器不响应”的情况 */
	uint8_t status;
	ret = i2c_read(i2c_dev_internal, &status, 1, AHT30_I2C_ADDR);
	if (ret == 0 && (status & 0x80)) {
		printk("AHT30 状态忙 (0x%02X)，尝试等待...\r\n", status);
		k_msleep(10);
	}

	/* 发送测量命令 */
	cmd[0] = AHT30_CMD_MEASURE;
	cmd[1] = AHT30_MEASURE_ARG1;
	cmd[2] = AHT30_MEASURE_ARG2;
	ret = i2c_write(i2c_dev_internal, cmd, 3, AHT30_I2C_ADDR);
	if (ret != 0) {
		printk("AHT30 测量命令发送失败: %d\r\n", ret);
		return ret;
	}

	/* 等待测量完成 (约 80ms) */
	k_msleep(80);

	/* 读取 7 字节数据 (状态 + 湿度 + 温度 + CRC) */
	ret = i2c_read(i2c_dev_internal, data, 7, AHT30_I2C_ADDR);
	if (ret != 0) {
		printk("AHT30 数据读取失败: %d\r\n", ret);
		return ret;
	}

	/* 检查状态位 (bit7=1 表示忙) */
	if (data[0] & 0x80) {
		printk("AHT30 传感器忙\r\n");
		return -EBUSY;
	}

	/* 解析湿度数据 (20位): data[1], data[2], data[3]高4位 */
	raw_humidity = ((uint32_t)data[1] << 12) | 
	               ((uint32_t)data[2] << 4) | 
	               ((uint32_t)data[3] >> 4);

	/* 解析温度数据 (20位): data[3]低4位, data[4], data[5] */
	raw_temperature = (((uint32_t)data[3] & 0x0F) << 16) | 
	                  ((uint32_t)data[4] << 8) | 
	                  (uint32_t)data[5];

	/* 转换为实际值 (放大100倍，避免浮点运算) */
	/* 湿度 = (raw / 2^20) * 100 * 100 = raw * 10000 / 1048576 */
	/* 温度 = ((raw / 2^20) * 200 - 50) * 100 = raw * 20000 / 1048576 - 5000 */
	*humidity = (int32_t)((raw_humidity * 10000ULL) / 1048576ULL);
	*temperature = (int32_t)((raw_temperature * 20000ULL) / 1048576ULL) - 5000;

	return 0;
}
