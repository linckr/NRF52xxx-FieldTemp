/**
 * @file spl06.h
 * @brief SPL06-001 气压传感器驱动头文件
 */

#ifndef SENSORS_SPL06_H_
#define SENSORS_SPL06_H_

#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SPL06-001/007 I2C 地址（据 Goertek 数据手册：由 SDO 决定，与 CSB 无关）
 * - SDO 接 GND → 0x76；SDO 接 VDD 或 NC → 0x77（默认）
 * - CSB 仅用于 I2C/SPI 选择（低=SPI），I2C 时 CSB 悬空或接 VDDIO
 * 本板/本模块 SDO 固定为 0x77 */
#define SPL06_I2C_ADDR     0x76

/* SPL06-001 寄存器地址 */
#define SPL06_REG_PRS_B2   0x00  /* 压力数据（MSB）*/
#define SPL06_REG_PRS_B1   0x01  /* 压力数据 */
#define SPL06_REG_PRS_B0   0x02  /* 压力数据（LSB）*/
#define SPL06_REG_TMP_B2   0x03  /* 温度数据（MSB）*/
#define SPL06_REG_TMP_B1   0x04  /* 温度数据 */
#define SPL06_REG_TMP_B0   0x05  /* 温度数据（LSB）*/
#define SPL06_REG_PRS_CFG  0x06  /* 压力配置寄存器 */
#define SPL06_REG_TMP_CFG  0x07  /* 温度配置寄存器 */
#define SPL06_REG_MEAS_CFG 0x08  /* 测量配置寄存器 */
#define SPL06_REG_CFG_REG  0x09  /* 配置寄存器 */
#define SPL06_REG_INT_STS  0x0A  /* 中断状态寄存器 */
#define SPL06_REG_FIFO_STS 0x0B  /* FIFO状态寄存器 */
#define SPL06_REG_RESET    0x0C  /* 复位寄存器 */
#define SPL06_REG_ID       0x0D  /* 产品ID寄存器 */
#define SPL06_REG_COEF     0x10  /* 校准系数起始地址（0x10-0x21）*/

/* MEAS_CFG 模式（bit[2:0]）：000=Idle, 001=压力, 010=温度, 011=压力+温度(单次), 111=连续压力+温度 */
#define SPL06_MEAS_CFG_IDLE            0x00
#define SPL06_MEAS_CFG_PRS             0x01
#define SPL06_MEAS_CFG_TMP             0x02
#define SPL06_MEAS_CFG_PRS_TMP_ONESHOT 0x03
#define SPL06_MEAS_CFG_CONT_PRS_TMP    0x07

/* SPL06-001 补偿计算：过采样 8 times 的缩放因子（datasheet Table 4）*/
#define SPL06_KP_OSR_8     7864320
#define SPL06_KT_OSR_8     7864320
#define SPL06_FP_SHIFT     20  /* Q20 定点 */

/* SPL06-001 校准系数结构 */
struct spl06_calib_coef {
	int16_t c0;   /* 12-bit 2's complement */
	int16_t c1;   /* 12-bit 2's complement */
	int32_t c00;  /* 20-bit 2's complement */
	int32_t c10;  /* 20-bit 2's complement */
	int16_t c01;
	int16_t c11;
	int16_t c20;
	int16_t c21;
	int16_t c30;
};

/**
 * @brief 初始化 SPL06-001 传感器（连续模式，默认）
 * @param i2c_dev I2C 设备指针
 * @return 0 成功，负数错误码
 */
int spl06_init(const struct device *i2c_dev);

/**
 * @brief 初始化 SPL06-001 传感器，并选择测量模式
 * @param i2c_dev I2C 设备指针
 * @param continuous true=连续模式(0x07)，false=单次模式(初始化后 MEAS_CFG=Idle，每次 read 时触发)
 * @return 0 成功，负数错误码
 */
int spl06_init_ex(const struct device *i2c_dev, bool continuous);

/**
 * @brief 读取 SPL06-001 压力和温度数据
 * 
 * @param pressure 压力输出（Pa）
 * @param temperature 温度输出（0.01°C，放大100倍）
 * @return 0 成功，负数错误码
 */
int spl06_read(int32_t *pressure, int32_t *temperature);

/**
 * @brief 检查校准系数是否已加载
 * @return true 已加载，false 未加载
 */
bool spl06_is_calib_loaded(void);

#ifdef __cplusplus
}
#endif

#endif /* SENSORS_SPL06_H_ */
