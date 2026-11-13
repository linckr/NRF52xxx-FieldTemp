/**
 * @file ltr390.c
 * @brief LTR-390UV 紫外线/环境光传感器驱动
 */

#include "ltr390.h"
#include <zephyr/sys/printk.h>

/* 模块内静态 I2C 设备指针 */
static const struct device *i2c_dev_internal;

/* 当前增益和分辨率配置 (用于计算) */
static ltr390_gain_t current_gain = LTR390_GAIN_3;
static ltr390_resolution_t current_resolution = LTR390_RESOLUTION_18BIT_100MS;

/* 获取 I2C 设备指针 */
const struct device *ltr390_get_i2c_dev(void)
{
	return i2c_dev_internal;
}

/* 写寄存器 */
static int ltr390_write_reg(uint8_t reg, uint8_t value)
{
	uint8_t buf[2] = {reg, value};
	return i2c_write(i2c_dev_internal, buf, 2, LTR390_I2C_ADDR);
}

/* 读寄存器 */
static int ltr390_read_reg(uint8_t reg, uint8_t *value)
{
	return i2c_write_read(i2c_dev_internal, LTR390_I2C_ADDR, 
	                      &reg, 1, value, 1);
}

/* LTR-390UV 初始化函数 */
int ltr390_init(const struct device *i2c_dev)
{
	uint8_t part_id;
	int ret;

	/* 保存 I2C 设备指针 */
	i2c_dev_internal = i2c_dev;

	if (!device_is_ready(i2c_dev)) {
		printk("LTR390: I2C 设备未就绪\r\n");
		return -ENODEV;
	}

	/* 等待传感器上电稳定 */
	k_msleep(10);

	/* 读取器件 ID */
	ret = ltr390_read_reg(LTR390_PART_ID, &part_id);
	if (ret != 0) {
		printk("LTR390 (地址 0x%02X) 无响应, 错误: %d\r\n", 
		       LTR390_I2C_ADDR, ret);
		return ret;
	}

	/* 验证器件 ID (应该是 0xB2) */
	if ((part_id >> 4) != 0x0B) {
		printk("LTR390 器件ID错误: 0x%02X (期望 0xB2)\r\n", part_id);
		return -EINVAL;
	}
	printk("LTR390 已检测到 (ID: 0x%02X)\r\n", part_id);

	/* 复位传感器 (写入 0x10 到主控制寄存器) */
	ret = ltr390_write_reg(LTR390_MAIN_CTRL, 0x10);
	if (ret != 0) {
		printk("LTR390 复位失败: %d\r\n", ret);
		return ret;
	}
	k_msleep(10);

	/* 使能传感器，默认 UVS 模式 */
	ret = ltr390_write_reg(LTR390_MAIN_CTRL, 
	                       LTR390_CTRL_EN | LTR390_CTRL_MODE_UVS);
	if (ret != 0) {
		printk("LTR390 使能失败: %d\r\n", ret);
		return ret;
	}

	/* 设置默认增益 (3x) */
	ret = ltr390_set_gain(LTR390_GAIN_3);
	if (ret != 0) {
		return ret;
	}

	/* 设置默认分辨率 (18位, 100ms) */
	ret = ltr390_set_resolution(LTR390_RESOLUTION_18BIT_100MS);
	if (ret != 0) {
		return ret;
	}

	printk("LTR390 传感器初始化完成\r\n");
	return 0;
}

/* 设置测量模式 */
int ltr390_set_mode(ltr390_mode_t mode)
{
	uint8_t ctrl_val;

	if (mode == LTR390_MODE_ALS) {
		ctrl_val = LTR390_CTRL_EN | LTR390_CTRL_MODE_ALS;
	} else {
		ctrl_val = LTR390_CTRL_EN | LTR390_CTRL_MODE_UVS;
	}

	int ret = ltr390_write_reg(LTR390_MAIN_CTRL, ctrl_val);
	if (ret != 0) {
		printk("LTR390 设置模式失败: %d\r\n", ret);
		return ret;
	}

	/* 模式切换后需要等待一个测量周期 */
	k_msleep(100);
	return 0;
}

/* 设置增益 */
int ltr390_set_gain(ltr390_gain_t gain)
{
	int ret = ltr390_write_reg(LTR390_ALS_UVS_GAIN, gain);
	if (ret != 0) {
		printk("LTR390 设置增益失败: %d\r\n", ret);
		return ret;
	}
	current_gain = gain;
	return 0;
}

/* 设置分辨率和测量速率 */
int ltr390_set_resolution(ltr390_resolution_t resolution)
{
	/* 测量速率寄存器格式: [7:4]=速率, [3:0]=分辨率 */
	/* 这里速率和分辨率设置为相同值 */
	uint8_t rate_val = (resolution << 4) | resolution;
	
	int ret = ltr390_write_reg(LTR390_ALS_UVS_MEAS_RATE, rate_val);
	if (ret != 0) {
		printk("LTR390 设置分辨率失败: %d\r\n", ret);
		return ret;
	}
	current_resolution = resolution;
	return 0;
}

/* 等待数据就绪 */
static int ltr390_wait_data_ready(void)
{
	uint8_t status;
	int retry = 20;  /* 最多等待 2 秒 */

	while (retry--) {
		int ret = ltr390_read_reg(LTR390_MAIN_STATUS, &status);
		if (ret != 0) {
			return ret;
		}

		if (status & LTR390_STATUS_DATA_READY) {
			return 0;
		}

		k_msleep(100);
	}

	printk("LTR390 等待数据就绪超时\r\n");
	return -ETIMEDOUT;
}

/* 读取环境光数据 */
int ltr390_read_als(uint32_t *als_data)
{
	uint8_t data[3];
	int ret;

	/* 检查 I2C 设备 */
	if (i2c_dev_internal == NULL || !device_is_ready(i2c_dev_internal)) {
		printk("LTR390 I2C 设备未就绪\r\n");
		return -ENODEV;
	}

	/* 切换到 ALS 模式 */
	ret = ltr390_set_mode(LTR390_MODE_ALS);
	if (ret != 0) {
		return ret;
	}

	/* 等待数据就绪 */
	ret = ltr390_wait_data_ready();
	if (ret != 0) {
		return ret;
	}

	/* 读取 3 字节 ALS 数据 */
	ret = i2c_write_read(i2c_dev_internal, LTR390_I2C_ADDR,
	                     (uint8_t[]){LTR390_ALS_DATA_0}, 1, data, 3);
	if (ret != 0) {
		printk("LTR390 ALS 数据读取失败: %d\r\n", ret);
		return ret;
	}

	/* 组合 20 位数据 */
	*als_data = ((uint32_t)data[2] << 16) | 
	            ((uint32_t)data[1] << 8) | 
	            (uint32_t)data[0];

	return 0;
}

/* 读取紫外线数据 */
int ltr390_read_uvs(uint32_t *uvs_data)
{
	uint8_t data[3];
	int ret;

	/* 检查 I2C 设备 */
	if (i2c_dev_internal == NULL || !device_is_ready(i2c_dev_internal)) {
		printk("LTR390 I2C 设备未就绪\r\n");
		return -ENODEV;
	}

	/* 切换到 UVS 模式 */
	ret = ltr390_set_mode(LTR390_MODE_UVS);
	if (ret != 0) {
		return ret;
	}

	/* 等待数据就绪 */
	ret = ltr390_wait_data_ready();
	if (ret != 0) {
		return ret;
	}

	/* 读取 3 字节 UVS 数据 */
	ret = i2c_write_read(i2c_dev_internal, LTR390_I2C_ADDR,
	                     (uint8_t[]){LTR390_UVS_DATA_0}, 1, data, 3);
	if (ret != 0) {
		printk("LTR390 UVS 数据读取失败: %d\r\n", ret);
		return ret;
	}

	/* 组合 20 位数据 */
	*uvs_data = ((uint32_t)data[2] << 16) | 
	            ((uint32_t)data[1] << 8) | 
	            (uint32_t)data[0];

	return 0;
}

/* 计算 UV 指数 */
int32_t ltr390_calculate_uvi(uint32_t uvs_data)
{
	/* UV 指数计算公式 (根据 LTR-390UV 数据手册):
	 * UVI = UVS / ((Gain / 18) * (IntegrationTime / 100)) / 2300
	 * 
	 * 这里简化计算，使用默认增益 3x 和 100ms 积分时间:
	 * UVI = UVS / 2300 * (18/3) * (100/100) = UVS / 383.33
	 * 
	 * 放大 100 倍返回: UVI_x100 = UVS * 100 / 383.33 ≈ UVS * 26 / 100
	 */
	
	/* 增益系数 */
	uint32_t gain_factor;
	switch (current_gain) {
		case LTR390_GAIN_1:  gain_factor = 18; break;
		case LTR390_GAIN_3:  gain_factor = 6;  break;
		case LTR390_GAIN_6:  gain_factor = 3;  break;
		case LTR390_GAIN_9:  gain_factor = 2;  break;
		case LTR390_GAIN_18: gain_factor = 1;  break;
		default: gain_factor = 6;
	}

	/* 积分时间系数 (相对于 100ms) */
	uint32_t time_factor;
	switch (current_resolution) {
		case LTR390_RESOLUTION_20BIT_400MS:  time_factor = 25;  break;
		case LTR390_RESOLUTION_19BIT_200MS:  time_factor = 50;  break;
		case LTR390_RESOLUTION_18BIT_100MS:  time_factor = 100; break;
		case LTR390_RESOLUTION_17BIT_50MS:   time_factor = 200; break;
		case LTR390_RESOLUTION_16BIT_25MS:   time_factor = 400; break;
		case LTR390_RESOLUTION_13BIT_12_5MS: time_factor = 800; break;
		default: time_factor = 100;
	}

	/* UVI = UVS * gain_factor * time_factor / 2300 / 100 (放大100倍) */
	int32_t uvi = (int32_t)((uvs_data * gain_factor * time_factor) / 230000ULL);
	
	return uvi;
}

/* 计算光照强度 (Lux) */
int32_t ltr390_calculate_lux(uint32_t als_data)
{
	/* Lux 计算公式 (根据 LTR-390UV 数据手册):
	 * Lux = 0.6 * ALS / ((Gain / 3) * (IntegrationTime / 100))
	 * 
	 * 放大 100 倍: Lux_x100 = 60 * ALS / ((Gain / 3) * (IntegrationTime / 100))
	 */
	
	/* 增益系数 */
	uint32_t gain_divisor;
	switch (current_gain) {
		case LTR390_GAIN_1:  gain_divisor = 3;  break;
		case LTR390_GAIN_3:  gain_divisor = 1;  break;
		case LTR390_GAIN_6:  gain_divisor = 2;  break;
		case LTR390_GAIN_9:  gain_divisor = 3;  break;
		case LTR390_GAIN_18: gain_divisor = 6;  break;
		default: gain_divisor = 1;
	}

	/* 积分时间系数 (相对于 100ms) */
	uint32_t time_divisor;
	switch (current_resolution) {
		case LTR390_RESOLUTION_20BIT_400MS:  time_divisor = 4;   break;
		case LTR390_RESOLUTION_19BIT_200MS:  time_divisor = 2;   break;
		case LTR390_RESOLUTION_18BIT_100MS:  time_divisor = 1;   break;
		case LTR390_RESOLUTION_17BIT_50MS:   time_divisor = 1;   break;
		case LTR390_RESOLUTION_16BIT_25MS:   time_divisor = 1;   break;
		case LTR390_RESOLUTION_13BIT_12_5MS: time_divisor = 1;   break;
		default: time_divisor = 1;
	}

	/* Lux_x100 = 60 * ALS * gain_divisor / time_divisor */
	int32_t lux = (int32_t)((als_data * 60ULL * gain_divisor) / time_divisor);
	
	return lux;
}
