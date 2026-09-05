/**
 * @file ltr390_example.c
 * @brief LTR-390UV 传感器使用示例
 * 
 * 本文件展示如何使用 LTR-390UV 驱动读取紫外线和环境光数据
 */

#include "ltr390.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>

/* I2C 设备节点 */
#define I2C_NODE DT_NODELABEL(i2c0)

/**
 * @brief LTR-390UV 测试示例
 * 
 * 演示如何初始化传感器并读取 UV 和 ALS 数据
 */
void ltr390_example(void)
{
	const struct device *i2c_dev;
	uint32_t uvs_data, als_data;
	int32_t uvi, lux;
	int ret;

	printk("\n========== LTR-390UV 传感器测试 ==========\n");

	/* 获取 I2C 设备 */
	i2c_dev = DEVICE_DT_GET(I2C_NODE);
	if (!device_is_ready(i2c_dev)) {
		printk("错误: I2C 设备未就绪\n");
		return;
	}

	/* 初始化 LTR-390UV */
	ret = ltr390_init(i2c_dev);
	if (ret != 0) {
		printk("错误: LTR-390UV 初始化失败 (%d)\n", ret);
		return;
	}

	/* 配置传感器参数 */
	printk("\n配置传感器参数...\n");
	
	/* 设置增益为 3x (适合室内环境) */
	ret = ltr390_set_gain(LTR390_GAIN_3);
	if (ret != 0) {
		printk("警告: 设置增益失败 (%d)\n", ret);
	}

	/* 设置分辨率为 18位/100ms (平衡速度和精度) */
	ret = ltr390_set_resolution(LTR390_RESOLUTION_18BIT_100MS);
	if (ret != 0) {
		printk("警告: 设置分辨率失败 (%d)\n", ret);
	}

	/* 循环读取数据 */
	printk("\n开始读取数据 (每 5 秒一次)...\n");
	printk("按 Ctrl+C 停止\n\n");

	while (1) {
		/* 读取紫外线数据 */
		ret = ltr390_read_uvs(&uvs_data);
		if (ret == 0) {
			uvi = ltr390_calculate_uvi(uvs_data);
			printk("UV 原始值: %6u  |  UV 指数: %d.%02d\n", 
			       uvs_data, uvi / 100, uvi % 100);
		} else {
			printk("错误: 读取 UV 数据失败 (%d)\n", ret);
		}

		/* 读取环境光数据 */
		ret = ltr390_read_als(&als_data);
		if (ret == 0) {
			lux = ltr390_calculate_lux(als_data);
			printk("ALS 原始值: %6u  |  光照强度: %d.%02d Lux\n", 
			       als_data, lux / 100, lux % 100);
		} else {
			printk("错误: 读取 ALS 数据失败 (%d)\n", ret);
		}

		printk("----------------------------------------\n");

		/* 等待 5 秒 */
		k_msleep(5000);
	}
}

/**
 * @brief 单次读取示例
 * 
 * 演示如何进行单次测量
 */
void ltr390_single_read_example(void)
{
	const struct device *i2c_dev;
	uint32_t uvs_data;
	int32_t uvi;
	int ret;

	/* 获取 I2C 设备 */
	i2c_dev = DEVICE_DT_GET(I2C_NODE);
	if (!device_is_ready(i2c_dev)) {
		printk("错误: I2C 设备未就绪\n");
		return;
	}

	/* 初始化传感器 */
	ret = ltr390_init(i2c_dev);
	if (ret != 0) {
		printk("错误: 初始化失败\n");
		return;
	}

	/* 读取一次 UV 数据 */
	ret = ltr390_read_uvs(&uvs_data);
	if (ret == 0) {
		uvi = ltr390_calculate_uvi(uvs_data);
		printk("当前 UV 指数: %d.%02d\n", uvi / 100, uvi % 100);
		
		/* UV 指数说明 */
		if (uvi < 300) {
			printk("UV 等级: 低 (可以安全待在户外)\n");
		} else if (uvi < 600) {
			printk("UV 等级: 中等 (需要防晒措施)\n");
		} else if (uvi < 800) {
			printk("UV 等级: 高 (必须采取防晒措施)\n");
		} else if (uvi < 1100) {
			printk("UV 等级: 很高 (避免在正午外出)\n");
		} else {
			printk("UV 等级: 极高 (尽量待在室内)\n");
		}
	}
}

/**
 * @brief 高级配置示例
 * 
 * 演示如何根据不同场景配置传感器
 */
void ltr390_advanced_config_example(void)
{
	const struct device *i2c_dev;
	int ret;

	i2c_dev = DEVICE_DT_GET(I2C_NODE);
	ret = ltr390_init(i2c_dev);
	if (ret != 0) {
		return;
	}

	printk("\n========== 场景配置示例 ==========\n");

	/* 场景 1: 室内环境光监测 */
	printk("\n场景 1: 室内环境光监测\n");
	printk("- 增益: 18x (高灵敏度)\n");
	printk("- 分辨率: 20位/400ms (高精度)\n");
	ltr390_set_gain(LTR390_GAIN_18);
	ltr390_set_resolution(LTR390_RESOLUTION_20BIT_400MS);

	/* 场景 2: 户外 UV 监测 */
	printk("\n场景 2: 户外 UV 监测\n");
	printk("- 增益: 1x (避免饱和)\n");
	printk("- 分辨率: 16位/25ms (快速响应)\n");
	ltr390_set_gain(LTR390_GAIN_1);
	ltr390_set_resolution(LTR390_RESOLUTION_16BIT_25MS);

	/* 场景 3: 低功耗模式 */
	printk("\n场景 3: 低功耗模式\n");
	printk("- 增益: 3x (平衡)\n");
	printk("- 分辨率: 13位/12.5ms (最快速度)\n");
	ltr390_set_gain(LTR390_GAIN_3);
	ltr390_set_resolution(LTR390_RESOLUTION_13BIT_12_5MS);
}
