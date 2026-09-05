/**
 * @file sht40.h
 * @brief SHT40 温湿度传感器驱动头文件
 */

#ifndef SENSORS_SHT40_H_
#define SENSORS_SHT40_H_

#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SHT40 I2C 地址 */
#define SHT40_I2C_ADDR_A    0x44  /* ADDR 引脚接地 (默认) */
#define SHT40_I2C_ADDR_B    0x45  /* ADDR 引脚接 VDD */

/* SHT40 命令定义 */
#define SHT40_CMD_MEASURE_HIGH      0xFD  /* 高精度测量 (8.2ms) */
#define SHT40_CMD_MEASURE_MED       0xF6  /* 中精度测量 (4.5ms) */
#define SHT40_CMD_MEASURE_LOW       0xE0  /* 低精度测量 (1.7ms) */
#define SHT40_CMD_READ_SERIAL       0x89  /* 读取序列号 */
#define SHT40_CMD_SOFT_RESET        0x94  /* 软复位 */

/* 加热器命令 (用于去除传感器表面冷凝) */
#define SHT40_CMD_HEAT_200MW_1S     0x39  /* 200mW 加热 1 秒 */
#define SHT40_CMD_HEAT_200MW_0_1S   0x32  /* 200mW 加热 0.1 秒 */
#define SHT40_CMD_HEAT_110MW_1S     0x2F  /* 110mW 加热 1 秒 */
#define SHT40_CMD_HEAT_110MW_0_1S   0x24  /* 110mW 加热 0.1 秒 */
#define SHT40_CMD_HEAT_20MW_1S      0x1E  /* 20mW 加热 1 秒 */
#define SHT40_CMD_HEAT_20MW_0_1S    0x15  /* 20mW 加热 0.1 秒 */

/* 测量精度模式 */
typedef enum {
	SHT40_PRECISION_HIGH = 0,   /* 高精度 (8.2ms, ±0.2°C, ±1.8%RH) */
	SHT40_PRECISION_MED  = 1,   /* 中精度 (4.5ms, ±0.3°C, ±2.5%RH) */
	SHT40_PRECISION_LOW  = 2,   /* 低精度 (1.7ms, ±0.4°C, ±3.5%RH) */
} sht40_precision_t;

/* 加热器配置 */
typedef enum {
	SHT40_HEATER_OFF = 0,       /* 不使用加热器 */
	SHT40_HEATER_200MW_1S,      /* 200mW, 1 秒 */
	SHT40_HEATER_200MW_0_1S,    /* 200mW, 0.1 秒 */
	SHT40_HEATER_110MW_1S,      /* 110mW, 1 秒 */
	SHT40_HEATER_110MW_0_1S,    /* 110mW, 0.1 秒 */
	SHT40_HEATER_20MW_1S,       /* 20mW, 1 秒 */
	SHT40_HEATER_20MW_0_1S,     /* 20mW, 0.1 秒 */
} sht40_heater_t;

/**
 * @brief 初始化 SHT40 传感器
 * 
 * @param i2c_dev I2C 设备指针
 * @param addr I2C 地址 (SHT40_I2C_ADDR_A 或 SHT40_I2C_ADDR_B)
 * @return 0 成功，负数错误码
 */
int sht40_init(const struct device *i2c_dev, uint8_t addr);

/**
 * @brief 软复位 SHT40 传感器
 * 
 * @return 0 成功，负数错误码
 */
int sht40_soft_reset(void);

/**
 * @brief 读取 SHT40 温湿度数据
 * 
 * @param temperature 温度输出（0.01°C，放大100倍）
 * @param humidity 湿度输出（0.01%RH，放大100倍）
 * @param precision 测量精度模式
 * @return 0 成功，负数错误码
 */
int sht40_read(int32_t *temperature, int32_t *humidity, sht40_precision_t precision);

/**
 * @brief 使用加热器进行测量
 * 
 * 加热器可以去除传感器表面的冷凝水，提高测量准确性
 * 注意：频繁使用加热器会缩短传感器寿命
 * 
 * @param temperature 温度输出（0.01°C，放大100倍）
 * @param humidity 湿度输出（0.01%RH，放大100倍）
 * @param heater 加热器配置
 * @return 0 成功，负数错误码
 */
int sht40_read_with_heater(int32_t *temperature, int32_t *humidity, 
                           sht40_heater_t heater);

/**
 * @brief 读取传感器序列号
 * 
 * @param serial 序列号输出 (32位)
 * @return 0 成功，负数错误码
 */
int sht40_read_serial(uint32_t *serial);

/**
 * @brief 获取 I2C 设备指针
 * @return I2C 设备指针
 */
const struct device *sht40_get_i2c_dev(void);

/**
 * @brief 获取当前配置的 I2C 地址
 * @return I2C 地址
 */
uint8_t sht40_get_i2c_addr(void);

#ifdef __cplusplus
}
#endif

#endif /* SENSORS_SHT40_H_ */
