/**
 * @file aht30.h
 * @brief AHT30 温湿度传感器驱动头文件
 */

#ifndef SENSORS_AHT30_H_
#define SENSORS_AHT30_H_

#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>

#ifdef __cplusplus
extern "C" {
#endif

/* AHT30 I2C 地址 */
#define AHT30_I2C_ADDR     0x38

/* AHT30 命令定义 */
#define AHT30_CMD_INIT     0xBE  /* 初始化命令 */
#define AHT30_CMD_MEASURE  0xAC  /* 触发测量命令 */
#define AHT30_MEASURE_ARG1 0x33  /* 测量命令参数1 */
#define AHT30_MEASURE_ARG2 0x00  /* 测量命令参数2 */

/**
 * @brief 初始化 AHT30 传感器
 * 
 * @param i2c_dev I2C 设备指针
 * @return 0 成功，负数错误码
 */
int aht30_init(const struct device *i2c_dev);

/**
 * @brief 读取 AHT30 温湿度数据
 * 
 * @param temperature 温度输出（0.01°C，放大100倍）
 * @param humidity 湿度输出（0.01%RH，放大100倍）
 * @return 0 成功，负数错误码
 */
int aht30_read(int32_t *temperature, int32_t *humidity);

/**
 * @brief 获取 I2C 设备指针
 * @return I2C 设备指针
 */
const struct device *aht30_get_i2c_dev(void);

#ifdef __cplusplus
}
#endif

#endif /* SENSORS_AHT30_H_ */
