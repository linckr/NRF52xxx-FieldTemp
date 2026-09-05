/**
 * @file spl06.c
 * @brief SPL06-001 气压传感器驱动
 */

#include "spl06.h"
#include <zephyr/sys/printk.h>

/* 模块内静态变量 */
static const struct device *i2c_dev_internal;
static struct spl06_calib_coef spl06_calib;
static bool spl06_calib_loaded = false;
static bool use_continuous = true;  /* true=连续模式，false=单次模式 */

/* 检查校准系数是否已加载 */
bool spl06_is_calib_loaded(void)
{
	return spl06_calib_loaded;
}

/* SPL06-001 初始化函数 */
int spl06_init(const struct device *i2c_dev)
{
	uint8_t reg_addr;
	uint8_t reg_data;
	int ret;

	/* 保存 I2C 设备指针 */
	i2c_dev_internal = i2c_dev;

	/* 检查 I2C 设备是否已初始化 */
	if (i2c_dev == NULL || !device_is_ready(i2c_dev)) {
		printk("SPL06 I2C 设备未就绪\r\n");
		return -ENODEV;
	}

	/* 等待传感器上电稳定 */
	k_msleep(10);

	/* 读取产品ID验证传感器 */
	reg_addr = SPL06_REG_ID;
	ret = i2c_write_read(i2c_dev, SPL06_I2C_ADDR, &reg_addr, 1, &reg_data, 1);
	if (ret != 0) {
		printk("SPL06 (地址 0x%02X) 无响应, 错误: %d\r\n", SPL06_I2C_ADDR, ret);
		return ret;
	}
	printk("SPL06 已检测到 (产品ID: 0x%02X)\r\n", reg_data);

	/* 软复位（写入0x09到复位寄存器）*/
	uint8_t reset_cmd[2] = {SPL06_REG_RESET, 0x09};
	ret = i2c_write(i2c_dev, reset_cmd, 2, SPL06_I2C_ADDR);
	if (ret != 0) {
		printk("SPL06 复位命令发送失败: %d\r\n", ret);
		return ret;
	}
	k_msleep(10);  /* 等待复位完成 */

	/* 等待 COEF_RDY 与 SENSOR_RDY（datasheet: MEAS_CFG bit7/bit6）*/
	/* 超时保护：最多等待 200ms */
	for (int i = 0; i < 200; i++) {
		reg_addr = SPL06_REG_MEAS_CFG;
		ret = i2c_write_read(i2c_dev, SPL06_I2C_ADDR, &reg_addr, 1, &reg_data, 1);
		if (ret == 0) {
			bool coef_rdy = (reg_data & BIT(7)) != 0;
			bool sensor_rdy = (reg_data & BIT(6)) != 0;
			if (coef_rdy && sensor_rdy) {
				break;
			}
		}
		k_msleep(1);
	}

	/* 读取校准系数（0x10-0x21，共18字节）*/
	uint8_t calib_data[18];
	reg_addr = SPL06_REG_COEF;
	ret = i2c_write_read(i2c_dev, SPL06_I2C_ADDR, &reg_addr, 1, calib_data, sizeof(calib_data));
	if (ret != 0) {
		printk("SPL06 校准系数读取失败: %d\r\n", ret);
		return ret;
	}

	/* 解析校准系数（参考 datasheet 8.11 / Table 10）*/
	/* c0/c1: 12-bit 2's complement */
	int16_t c0_u12 = (int16_t)(((uint16_t)calib_data[0] << 4) | ((calib_data[1] >> 4) & 0x0F));
	int16_t c1_u12 = (int16_t)(((uint16_t)(calib_data[1] & 0x0F) << 8) | calib_data[2]);
	if (c0_u12 & 0x0800) { c0_u12 |= 0xF000; }
	if (c1_u12 & 0x0800) { c1_u12 |= 0xF000; }
	spl06_calib.c0 = c0_u12;
	spl06_calib.c1 = c1_u12;

	/* c00/c10: 20-bit 2's complement */
	int32_t c00_u20 = (int32_t)(((uint32_t)calib_data[3] << 12) |
	                            ((uint32_t)calib_data[4] << 4) |
	                            ((calib_data[5] >> 4) & 0x0F));
	int32_t c10_u20 = (int32_t)((((uint32_t)calib_data[5] & 0x0F) << 16) |
	                            ((uint32_t)calib_data[6] << 8) |
	                            (uint32_t)calib_data[7]);
	if (c00_u20 & 0x00080000) { c00_u20 |= 0xFFF00000; }
	if (c10_u20 & 0x00080000) { c10_u20 |= 0xFFF00000; }
	spl06_calib.c00 = c00_u20;
	spl06_calib.c10 = c10_u20;

	/* 16-bit 2's complement */
	spl06_calib.c01 = (int16_t)((uint16_t)calib_data[8] << 8) | calib_data[9];
	spl06_calib.c11 = (int16_t)((uint16_t)calib_data[10] << 8) | calib_data[11];
	spl06_calib.c20 = (int16_t)((uint16_t)calib_data[12] << 8) | calib_data[13];
	spl06_calib.c21 = (int16_t)((uint16_t)calib_data[14] << 8) | calib_data[15];
	spl06_calib.c30 = (int16_t)((uint16_t)calib_data[16] << 8) | calib_data[17];

	spl06_calib_loaded = true;
	printk("SPL06 校准系数已加载\r\n");

	/* 配置测量模式 */
	/* 压力配置：连续测量，过采样率8（精度与速度平衡）*/
	uint8_t prs_cfg[2] = {SPL06_REG_PRS_CFG, 0x03};  /* bit[2:0] = 011 (过采样率8) */
	ret = i2c_write(i2c_dev, prs_cfg, 2, SPL06_I2C_ADDR);
	if (ret != 0) {
		printk("SPL06 压力配置失败: %d\r\n", ret);
		return ret;
	}

	/* 温度配置：连续测量，过采样率8，使用芯片内部温度（气压温度补偿用）*/
	/* bit[2:0]=011(过采样8), bit7=0 内部温度；若板子接外部温源则改为 0x83 */
	uint8_t tmp_cfg[2] = {SPL06_REG_TMP_CFG, 0x03};
	ret = i2c_write(i2c_dev, tmp_cfg, 2, SPL06_I2C_ADDR);
	if (ret != 0) {
		printk("SPL06 温度配置失败: %d\r\n", ret);
		return ret;
	}

	/* 测量配置：默认使用单次模式 (0x00)，每次读取时再触发 */
	use_continuous = false;
	uint8_t meas_cfg[2] = {SPL06_REG_MEAS_CFG, SPL06_MEAS_CFG_IDLE};
	ret = i2c_write(i2c_dev, meas_cfg, 2, SPL06_I2C_ADDR);
	if (ret != 0) {
		printk("SPL06 测量配置失败: %d\r\n", ret);
		return ret;
	}

	printk("SPL06 传感器初始化完成（单次模式）\r\n");
	return 0;
}

/* 扩展初始化：可选择连续或单次模式 */
int spl06_init_ex(const struct device *i2c_dev, bool continuous)
{
	int ret = spl06_init(i2c_dev);
	if (ret != 0) {
		return ret;
	}
	use_continuous = continuous;
	if (continuous) {
		uint8_t meas_cfg[2] = {SPL06_REG_MEAS_CFG, SPL06_MEAS_CFG_CONT_PRS_TMP};
		ret = i2c_write(i2c_dev, meas_cfg, 2, SPL06_I2C_ADDR);
		printk("SPL06 模式：连续\r\n");
	} else {
		uint8_t meas_cfg[2] = {SPL06_REG_MEAS_CFG, SPL06_MEAS_CFG_IDLE};
		ret = i2c_write(i2c_dev, meas_cfg, 2, SPL06_I2C_ADDR);
		printk("SPL06 模式：单次（每次 read 触发 0x03）\r\n");
	}
	if (ret != 0) {
		printk("SPL06 测量配置失败: %d\r\n", ret);
		return ret;
	}
	return 0;
}

/* 定点乘法辅助函数 */
static inline int64_t mul_q20(int64_t a_q, int64_t b_q)
{
	return (a_q * b_q) >> SPL06_FP_SHIFT;
}

/* SPL06-001 读取压力和温度函数（按手册补偿流程输出 Pa 与 0.01°C）*/
int spl06_read(int32_t *pressure, int32_t *temperature)
{
	uint8_t reg_addr;
	uint8_t data[3];
	uint8_t meas_cfg;
	int32_t raw_pressure, raw_temperature;
	int ret;
	int timeout;

	/* 检查 I2C 设备是否已初始化 */
	if (i2c_dev_internal == NULL || !device_is_ready(i2c_dev_internal)) {
		printk("SPL06 I2C 设备未就绪\r\n");
		return -ENODEV;
	}
	if (!spl06_calib_loaded) {
		printk("SPL06 校准系数未加载\r\n");
		return -EINVAL;
	}

	if (!use_continuous) {
		/* ---------- 单次模式：压力/温度分开触发、分别等待再读，保证每次都是新转换结果 ---------- */
		uint8_t idle_cmd[2] = {SPL06_REG_MEAS_CFG, SPL06_MEAS_CFG_IDLE};
		ret = i2c_write(i2c_dev_internal, idle_cmd, 2, SPL06_I2C_ADDR);
		if (ret != 0) {
			printk("SPL06 单次模式 Idle 写入失败: %d\r\n", ret);
			return ret;
		}
		k_msleep(2);

		/* 先触发仅压力 (001)，等转换完成再读压力 */
		uint8_t prs_cmd[2] = {SPL06_REG_MEAS_CFG, SPL06_MEAS_CFG_PRS};
		ret = i2c_write(i2c_dev_internal, prs_cmd, 2, SPL06_I2C_ADDR);
		if (ret != 0) {
			printk("SPL06 单次模式触发压力失败: %d\r\n", ret);
			return ret;
		}
		k_msleep(32);
		reg_addr = SPL06_REG_PRS_B2;
		ret = i2c_write_read(i2c_dev_internal, SPL06_I2C_ADDR, &reg_addr, 1, data, 3);
		if (ret != 0) {
			printk("SPL06 单次模式压力读取失败: %d\r\n", ret);
			return ret;
		}
		raw_pressure = (int32_t)((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | (uint32_t)data[2];
		if (raw_pressure & 0x00800000) { raw_pressure |= 0xFF000000; }

		/* 再触发仅温度 (010)，等转换完成再读温度 */
		uint8_t tmp_cmd[2] = {SPL06_REG_MEAS_CFG, SPL06_MEAS_CFG_TMP};
		ret = i2c_write(i2c_dev_internal, tmp_cmd, 2, SPL06_I2C_ADDR);
		if (ret != 0) {
			printk("SPL06 单次模式触发温度失败: %d\r\n", ret);
			return ret;
		}
		k_msleep(32);
		reg_addr = SPL06_REG_TMP_B2;
		ret = i2c_write_read(i2c_dev_internal, SPL06_I2C_ADDR, &reg_addr, 1, data, 3);
		if (ret != 0) {
			printk("SPL06 单次模式温度读取失败: %d\r\n", ret);
			return ret;
		}
		raw_temperature = (int32_t)((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | (uint32_t)data[2];
		if (raw_temperature & 0x00800000) { raw_temperature |= 0xFF000000; }
	} else {
		/* ---------- 连续模式（已废弃建议，但保留代码兼容） ---------- */
		/* 为确保数据同步，即使在连续模式下，也建议等待 READY 位 */
		timeout = 100;
		while (timeout > 0) {
			reg_addr = SPL06_REG_MEAS_CFG;
			ret = i2c_write_read(i2c_dev_internal, SPL06_I2C_ADDR, &reg_addr, 1, &meas_cfg, 1);
			if (ret == 0 && (meas_cfg & 0x30) == 0x30) {
				break;
			}
			k_msleep(2);
			timeout--;
		}
		if (timeout <= 0) {
			printk("SPL06 等待数据就绪超时 (MEAS_CFG=0x%02X)\r\n", meas_cfg);
		}

		reg_addr = SPL06_REG_PRS_B2;
		ret = i2c_write_read(i2c_dev_internal, SPL06_I2C_ADDR, &reg_addr, 1, data, 3);
		if (ret != 0) {
			printk("SPL06 压力数据读取失败: %d\r\n", ret);
			return ret;
		}
		raw_pressure = (int32_t)((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | (uint32_t)data[2];
		if (raw_pressure & 0x00800000) { raw_pressure |= 0xFF000000; }

		reg_addr = SPL06_REG_TMP_B2;
		ret = i2c_write_read(i2c_dev_internal, SPL06_I2C_ADDR, &reg_addr, 1, data, 3);
		if (ret != 0) {
			printk("SPL06 温度数据读取失败: %d\r\n", ret);
			return ret;
		}
		raw_temperature = (int32_t)((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | (uint32_t)data[2];
		if (raw_temperature & 0x00800000) { raw_temperature |= 0xFF000000; }
	}

	/* 选择缩放因子（当前配置为过采样 8 times）*/
	const int32_t kP = SPL06_KP_OSR_8;
	const int32_t kT = SPL06_KT_OSR_8;
	const int s = SPL06_FP_SHIFT;

	/* 定点缩放：Praw_sc / Traw_sc 用 Q20 表示 */
	int64_t p_q = ((int64_t)raw_pressure << s) / kP;     /* Q20 */
	int64_t t_q = ((int64_t)raw_temperature << s) / kT;  /* Q20 */

	/* 温度补偿：Tcomp(°C) = c0*0.5 + c1*Traw_sc */
	/* 输出为 0.01°C（centi-degC）*/
	int64_t t_centi = (int64_t)spl06_calib.c0 * 50;  /* c0*0.5*100 */
	t_centi += ((int64_t)spl06_calib.c1 * t_q * 100) >> s;
	*temperature = (int32_t)t_centi;

	/* 压力补偿（datasheet 5.6.1）*/
	/* Pcomp(Pa) = c00 + Praw_sc*(c10 + Praw_sc*(c20 + Praw_sc*c30)) */
	/*           + Traw_sc*c01 + Traw_sc*Praw_sc*(c11 + Praw_sc*c21) */
	int64_t p2_q = mul_q20(p_q, p_q);      /* Q20 */
	int64_t p3_q = mul_q20(p2_q, p_q);     /* Q20 */
	int64_t tp_q = mul_q20(t_q, p_q);      /* Q20 */
	int64_t tp2_q = mul_q20(tp_q, p_q);    /* Q20  (T*P^2) */

	int64_t p_pa = (int64_t)spl06_calib.c00;
	p_pa += ((int64_t)spl06_calib.c10 * p_q) >> s;
	p_pa += ((int64_t)spl06_calib.c20 * p2_q) >> s;
	p_pa += ((int64_t)spl06_calib.c30 * p3_q) >> s;
	p_pa += ((int64_t)spl06_calib.c01 * t_q) >> s;
	p_pa += ((int64_t)spl06_calib.c11 * tp_q) >> s;
	p_pa += ((int64_t)spl06_calib.c21 * tp2_q) >> s;

	*pressure = (int32_t)p_pa;

	/* 海平面附近约 100m 高度：气压应在约 100000~101500 Pa（1000~1015 hPa），异常时打日志便于排查 */
	if (p_pa < 90000 || p_pa > 110000) {
		printk("[SPL06] 气压异常: %d Pa (raw_p=%d, raw_t=%d)，海平面附近应为约 100000~101500 Pa\r\n",
		       (int32_t)p_pa, raw_pressure, raw_temperature);
	}

	return 0;
}
