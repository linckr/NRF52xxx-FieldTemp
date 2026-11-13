/**
 * @file ltr390.h
 * @brief LTR-390UV 紫外线/环境光传感器驱动头文件
 */

#ifndef SENSORS_LTR390_H_
#define SENSORS_LTR390_H_

#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>

#ifdef __cplusplus
extern "C" {
#endif

/* LTR-390UV I2C 地址 */
#define LTR390_I2C_ADDR         0x53

/* 寄存器地址 */
#define LTR390_MAIN_CTRL        0x00  /* 主控制寄存器 */
#define LTR390_ALS_UVS_MEAS_RATE 0x04 /* 测量速率寄存器 */
#define LTR390_ALS_UVS_GAIN     0x05  /* 增益寄存器 */
#define LTR390_PART_ID          0x06  /* 器件ID寄存器 */
#define LTR390_MAIN_STATUS      0x07  /* 状态寄存器 */
#define LTR390_ALS_DATA_0       0x0D  /* ALS数据低字节 */
#define LTR390_ALS_DATA_1       0x0E  /* ALS数据中字节 */
#define LTR390_ALS_DATA_2       0x0F  /* ALS数据高字节 */
#define LTR390_UVS_DATA_0       0x10  /* UVS数据低字节 */
#define LTR390_UVS_DATA_1       0x11  /* UVS数据中字节 */
#define LTR390_UVS_DATA_2       0x12  /* UVS数据高字节 */
#define LTR390_INT_CFG          0x19  /* 中断配置寄存器 */
#define LTR390_INT_PST          0x1A  /* 中断持续寄存器 */
#define LTR390_ALS_THRES_UP_0   0x21  /* ALS上阈值低字节 */
#define LTR390_ALS_THRES_LOW_0  0x24  /* ALS下阈值低字节 */
#define LTR390_UVS_THRES_UP_0   0x27  /* UVS上阈值低字节 */
#define LTR390_UVS_THRES_LOW_0  0x2A  /* UVS下阈值低字节 */

/* MAIN_CTRL 寄存器位定义 */
#define LTR390_CTRL_EN          (1 << 1)  /* 使能位 */
#define LTR390_CTRL_MODE_ALS    (1 << 3)  /* ALS模式 */
#define LTR390_CTRL_MODE_UVS    (0 << 3)  /* UVS模式 */

/* MAIN_STATUS 寄存器位定义 */
#define LTR390_STATUS_DATA_READY (1 << 3) /* 数据就绪位 */

/* 测量速率定义 (分辨率和测量速率) */
typedef enum {
	LTR390_RESOLUTION_20BIT_400MS = 0x00,  /* 20位, 400ms */
	LTR390_RESOLUTION_19BIT_200MS = 0x01,  /* 19位, 200ms */
	LTR390_RESOLUTION_18BIT_100MS = 0x02,  /* 18位, 100ms (默认) */
	LTR390_RESOLUTION_17BIT_50MS  = 0x03,  /* 17位, 50ms */
	LTR390_RESOLUTION_16BIT_25MS  = 0x04,  /* 16位, 25ms */
	LTR390_RESOLUTION_13BIT_12_5MS = 0x05, /* 13位, 12.5ms */
} ltr390_resolution_t;

/* 增益定义 */
typedef enum {
	LTR390_GAIN_1  = 0x00,  /* 增益 1x */
	LTR390_GAIN_3  = 0x01,  /* 增益 3x (默认) */
	LTR390_GAIN_6  = 0x02,  /* 增益 6x */
	LTR390_GAIN_9  = 0x03,  /* 增益 9x */
	LTR390_GAIN_18 = 0x04,  /* 增益 18x */
} ltr390_gain_t;

/* 测量模式 */
typedef enum {
	LTR390_MODE_ALS = 0,  /* 环境光模式 */
	LTR390_MODE_UVS = 1,  /* 紫外线模式 */
} ltr390_mode_t;

/**
 * @brief 初始化 LTR-390UV 传感器
 * 
 * @param i2c_dev I2C 设备指针
 * @return 0 成功，负数错误码
 */
int ltr390_init(const struct device *i2c_dev);

/**
 * @brief 设置测量模式
 * 
 * @param mode 测量模式 (ALS 或 UVS)
 * @return 0 成功，负数错误码
 */
int ltr390_set_mode(ltr390_mode_t mode);

/**
 * @brief 设置增益
 * 
 * @param gain 增益值
 * @return 0 成功，负数错误码
 */
int ltr390_set_gain(ltr390_gain_t gain);

/**
 * @brief 设置分辨率和测量速率
 * 
 * @param resolution 分辨率/测量速率
 * @return 0 成功，负数错误码
 */
int ltr390_set_resolution(ltr390_resolution_t resolution);

/**
 * @brief 读取环境光数据 (ALS)
 * 
 * @param als_data 环境光原始数据输出
 * @return 0 成功，负数错误码
 */
int ltr390_read_als(uint32_t *als_data);

/**
 * @brief 读取紫外线数据 (UVS)
 * 
 * @param uvs_data 紫外线原始数据输出
 * @return 0 成功，负数错误码
 */
int ltr390_read_uvs(uint32_t *uvs_data);

/**
 * @brief 计算 UV 指数
 * 
 * @param uvs_data 紫外线原始数据
 * @return UV 指数 (放大100倍，例如 250 表示 2.50)
 */
int32_t ltr390_calculate_uvi(uint32_t uvs_data);

/**
 * @brief 计算光照强度 (Lux)
 * 
 * @param als_data 环境光原始数据
 * @return 光照强度 (Lux，放大100倍)
 */
int32_t ltr390_calculate_lux(uint32_t als_data);

/**
 * @brief 获取 I2C 设备指针
 * @return I2C 设备指针
 */
const struct device *ltr390_get_i2c_dev(void);

#ifdef __cplusplus
}
#endif

#endif /* SENSORS_LTR390_H_ */
