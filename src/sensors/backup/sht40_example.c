/**
 * @file sht40_example.c
 * @brief SHT40 温湿度传感器使用示例
 * 
 * 本文件展示如何使用 SHT40 驱动读取温湿度数据
 */

#include "sht40.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>

/* I2C 设备节点 */
#define I2C_NODE DT_NODELABEL(i2c0)

/**
 * @brief SHT40 基础测试示例
 * 
 * 演示如何初始化传感器并读取温湿度数据
 */
void sht40_basic_example(void)
{
	const struct device *i2c_dev;
	int32_t temperature, humidity;
	int ret;

	printk("\n========== SHT40 传感器基础测试 ==========\n");

	/* 获取 I2C 设备 */
	i2c_dev = DEVICE_DT_GET(I2C_NODE);
	if (!device_is_ready(i2c_dev)) {
		printk("错误: I2C 设备未就绪\n");
		return;
	}

	/* 初始化 SHT40 (使用默认地址 0x44) */
	ret = sht40_init(i2c_dev, SHT40_I2C_ADDR_A);
	if (ret != 0) {
		printk("错误: SHT40 初始化失败 (%d)\n", ret);
		return;
	}

	/* 循环读取数据 */
	printk("\n开始读取数据 (每 2 秒一次)...\n");
	printk("按 Ctrl+C 停止\n\n");

	while (1) {
		/* 使用高精度模式读取 */
		ret = sht40_read(&temperature, &humidity, SHT40_PRECISION_HIGH);
		if (ret == 0) {
			printk("温度: %3d.%02d °C  |  湿度: %2d.%02d %%RH\n",
			       temperature / 100, abs(temperature % 100),
			       humidity / 100, humidity % 100);
		} else {
			printk("错误: 读取数据失败 (%d)\n", ret);
		}

		/* 等待 2 秒 */
		k_msleep(2000);
	}
}

/**
 * @brief 精度对比示例
 * 
 * 演示不同精度模式的测量时间和结果
 */
void sht40_precision_comparison(void)
{
	const struct device *i2c_dev;
	int32_t temperature, humidity;
	uint32_t start_time, elapsed_time;
	int ret;

	printk("\n========== SHT40 精度模式对比 ==========\n");

	i2c_dev = DEVICE_DT_GET(I2C_NODE);
	ret = sht40_init(i2c_dev, SHT40_I2C_ADDR_A);
	if (ret != 0) {
		return;
	}

	/* 高精度模式 */
	printk("\n1. 高精度模式 (±0.2°C, ±1.8%%RH):\n");
	start_time = k_uptime_get_32();
	ret = sht40_read(&temperature, &humidity, SHT40_PRECISION_HIGH);
	elapsed_time = k_uptime_get_32() - start_time;
	if (ret == 0) {
		printk("   温度: %d.%02d °C, 湿度: %d.%02d %%RH\n",
		       temperature / 100, abs(temperature % 100),
		       humidity / 100, humidity % 100);
		printk("   测量时间: %u ms\n", elapsed_time);
	}

	k_msleep(100);

	/* 中精度模式 */
	printk("\n2. 中精度模式 (±0.3°C, ±2.5%%RH):\n");
	start_time = k_uptime_get_32();
	ret = sht40_read(&temperature, &humidity, SHT40_PRECISION_MED);
	elapsed_time = k_uptime_get_32() - start_time;
	if (ret == 0) {
		printk("   温度: %d.%02d °C, 湿度: %d.%02d %%RH\n",
		       temperature / 100, abs(temperature % 100),
		       humidity / 100, humidity % 100);
		printk("   测量时间: %u ms\n", elapsed_time);
	}

	k_msleep(100);

	/* 低精度模式 */
	printk("\n3. 低精度模式 (±0.4°C, ±3.5%%RH):\n");
	start_time = k_uptime_get_32();
	ret = sht40_read(&temperature, &humidity, SHT40_PRECISION_LOW);
	elapsed_time = k_uptime_get_32() - start_time;
	if (ret == 0) {
		printk("   温度: %d.%02d °C, 湿度: %d.%02d %%RH\n",
		       temperature / 100, abs(temperature % 100),
		       humidity / 100, humidity % 100);
		printk("   测量时间: %u ms\n", elapsed_time);
	}

	printk("\n结论: 精度越高，测量时间越长，但数据更准确\n");
}

/**
 * @brief 加热器功能示例
 * 
 * 演示如何使用加热器去除传感器表面冷凝
 */
void sht40_heater_example(void)
{
	const struct device *i2c_dev;
	int32_t temp_before, humi_before;
	int32_t temp_after, humi_after;
	int ret;

	printk("\n========== SHT40 加热器功能测试 ==========\n");

	i2c_dev = DEVICE_DT_GET(I2C_NODE);
	ret = sht40_init(i2c_dev, SHT40_I2C_ADDR_A);
	if (ret != 0) {
		return;
	}

	/* 加热前测量 */
	printk("\n加热前测量:\n");
	ret = sht40_read(&temp_before, &humi_before, SHT40_PRECISION_HIGH);
	if (ret == 0) {
		printk("温度: %d.%02d °C, 湿度: %d.%02d %%RH\n",
		       temp_before / 100, abs(temp_before % 100),
		       humi_before / 100, humi_before % 100);
	}

	/* 使用加热器 (200mW, 1秒) */
	printk("\n使用加热器 (200mW, 1秒)...\n");
	ret = sht40_read_with_heater(&temp_after, &humi_after, 
	                             SHT40_HEATER_200MW_1S);
	if (ret == 0) {
		printk("加热后测量:\n");
		printk("温度: %d.%02d °C (变化: %+d.%02d °C)\n",
		       temp_after / 100, abs(temp_after % 100),
		       (temp_after - temp_before) / 100,
		       abs((temp_after - temp_before) % 100));
		printk("湿度: %d.%02d %%RH (变化: %+d.%02d %%RH)\n",
		       humi_after / 100, humi_after % 100,
		       (humi_after - humi_before) / 100,
		       abs((humi_after - humi_before) % 100));
	}

	printk("\n说明: 加热器会提高温度并降低湿度读数\n");
	printk("用途: 去除传感器表面冷凝，提高测量准确性\n");
	printk("注意: 频繁使用会缩短传感器寿命\n");
}

/**
 * @brief 序列号读取示例
 * 
 * 演示如何读取传感器唯一序列号
 */
void sht40_serial_example(void)
{
	const struct device *i2c_dev;
	uint32_t serial;
	int ret;

	printk("\n========== SHT40 序列号读取 ==========\n");

	i2c_dev = DEVICE_DT_GET(I2C_NODE);
	ret = sht40_init(i2c_dev, SHT40_I2C_ADDR_A);
	if (ret != 0) {
		return;
	}

	/* 读取序列号 */
	ret = sht40_read_serial(&serial);
	if (ret == 0) {
		printk("\n传感器序列号: 0x%08X\n", serial);
		printk("十进制: %u\n", serial);
		printk("\n用途: 可用于设备识别和追溯\n");
	} else {
		printk("错误: 序列号读取失败 (%d)\n", ret);
	}
}

/**
 * @brief 低功耗应用示例
 * 
 * 演示如何在低功耗场景下使用 SHT40
 */
void sht40_low_power_example(void)
{
	const struct device *i2c_dev;
	int32_t temperature, humidity;
	int ret;

	printk("\n========== SHT40 低功耗应用示例 ==========\n");

	i2c_dev = DEVICE_DT_GET(I2C_NODE);
	ret = sht40_init(i2c_dev, SHT40_I2C_ADDR_A);
	if (ret != 0) {
		return;
	}

	printk("\n低功耗策略:\n");
	printk("1. 使用低精度模式 (最快 1.7ms)\n");
	printk("2. 延长采样间隔 (例如每 60 秒一次)\n");
	printk("3. 不使用加热器功能\n");

	printk("\n开始低功耗采样 (每 60 秒一次)...\n\n");

	while (1) {
		/* 使用低精度模式，快速测量 */
		ret = sht40_read(&temperature, &humidity, SHT40_PRECISION_LOW);
		if (ret == 0) {
			printk("[%u] 温度: %d.%02d °C, 湿度: %d.%02d %%RH\n",
			       k_uptime_get_32() / 1000,
			       temperature / 100, abs(temperature % 100),
			       humidity / 100, humidity % 100);
		}

		/* 长时间休眠，降低功耗 */
		k_msleep(60000);  /* 60 秒 */
	}
}

/**
 * @brief 温湿度报警示例
 * 
 * 演示如何实现温湿度阈值报警
 */
void sht40_alarm_example(void)
{
	const struct device *i2c_dev;
	int32_t temperature, humidity;
	int ret;

	/* 阈值设置 (放大 100 倍) */
	const int32_t TEMP_HIGH_THRESHOLD = 3000;   /* 30.00 °C */
	const int32_t TEMP_LOW_THRESHOLD = 1500;    /* 15.00 °C */
	const int32_t HUMI_HIGH_THRESHOLD = 7000;   /* 70.00 %RH */
	const int32_t HUMI_LOW_THRESHOLD = 3000;    /* 30.00 %RH */

	printk("\n========== SHT40 温湿度报警示例 ==========\n");

	i2c_dev = DEVICE_DT_GET(I2C_NODE);
	ret = sht40_init(i2c_dev, SHT40_I2C_ADDR_A);
	if (ret != 0) {
		return;
	}

	printk("\n报警阈值设置:\n");
	printk("温度: %d.%02d ~ %d.%02d °C\n",
	       TEMP_LOW_THRESHOLD / 100, TEMP_LOW_THRESHOLD % 100,
	       TEMP_HIGH_THRESHOLD / 100, TEMP_HIGH_THRESHOLD % 100);
	printk("湿度: %d.%02d ~ %d.%02d %%RH\n\n",
	       HUMI_LOW_THRESHOLD / 100, HUMI_LOW_THRESHOLD % 100,
	       HUMI_HIGH_THRESHOLD / 100, HUMI_HIGH_THRESHOLD % 100);

	while (1) {
		ret = sht40_read(&temperature, &humidity, SHT40_PRECISION_HIGH);
		if (ret == 0) {
			printk("温度: %d.%02d °C, 湿度: %d.%02d %%RH",
			       temperature / 100, abs(temperature % 100),
			       humidity / 100, humidity % 100);

			/* 检查温度报警 */
			if (temperature > TEMP_HIGH_THRESHOLD) {
				printk(" [警告: 温度过高!]");
			} else if (temperature < TEMP_LOW_THRESHOLD) {
				printk(" [警告: 温度过低!]");
			}

			/* 检查湿度报警 */
			if (humidity > HUMI_HIGH_THRESHOLD) {
				printk(" [警告: 湿度过高!]");
			} else if (humidity < HUMI_LOW_THRESHOLD) {
				printk(" [警告: 湿度过低!]");
			}

			printk("\n");
		}

		k_msleep(5000);
	}
}
