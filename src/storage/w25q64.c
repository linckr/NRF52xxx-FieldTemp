/**
 * @file w25q64.c
 * @brief W25Q64 SPI Flash 驱动
 */

#include "w25q64.h"
#include <zephyr/devicetree.h>
#include <zephyr/sys/printk.h>
#include <string.h>

/* W25Q64 SPI设备 */
static const struct device *w25q64_spi_dev;

/* W25Q64 SPI配置：Mode 0 (CPOL=0, CPHA=0)，与 W25Q64 规格一致；不设置 CPOL/CPHA 位即为 mode 0 */
static struct spi_config w25q64_spi_cfg = {
	.frequency = 4000000,  /* 4MHz - 降低频率提高稳定性 */
	.operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_TRANSFER_MSB,
};

/* W25Q64 低功耗状态标志 */
static bool w25q64_in_sleep = false;

/* W25Q64 全局互斥锁 */
K_MUTEX_DEFINE(w25q64_mutex);

/* CS控制 */
static void w25q64_cs_select(void)
{
	const struct device *gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
	gpio_pin_set(gpio_dev, W25Q64_CS_PIN, 0);
}

static void w25q64_cs_deselect(void)
{
	const struct device *gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
	gpio_pin_set(gpio_dev, W25Q64_CS_PIN, 1);
}

/* 获取互斥锁 */
struct k_mutex *w25q64_get_mutex(void)
{
	return &w25q64_mutex;
}

/* 检查是否已就绪 */
bool w25q64_is_ready(void)
{
	return (w25q64_spi_dev != NULL && device_is_ready(w25q64_spi_dev));
}

/* 读取状态寄存器1 */
int w25q64_read_status_reg1(uint8_t *status)
{
	uint8_t tx_data[2] = {W25Q64_CMD_READ_STATUS_REG1, 0xFF};
	uint8_t rx_data[2] = {0};
	
	const struct spi_buf tx_buf = {
		.buf = tx_data,
		.len = 2,
	};
	const struct spi_buf_set tx_bufs = {
		.buffers = &tx_buf,
		.count = 1,
	};
	struct spi_buf rx_buf = {
		.buf = rx_data,
		.len = 2,
	};
	const struct spi_buf_set rx_bufs = {
		.buffers = &rx_buf,
		.count = 1,
	};

	w25q64_cs_select();
	int ret = spi_transceive(w25q64_spi_dev, &w25q64_spi_cfg, &tx_bufs, &rx_bufs);
	w25q64_cs_deselect();
	
	if (ret == 0) {
		*status = rx_data[1];
	}
	return ret;
}

/* 等待就绪 */
int w25q64_wait_ready(void)
{
	uint8_t status;
	
	for (int i = 0; i < 3000; i++) {
		int ret = w25q64_read_status_reg1(&status);
		if (ret != 0) {
			return ret;
		}
		if ((status & 0x01) == 0) {
			return 0;
		}
		if (i % 5 == 0) {
			k_yield();
		}
		k_msleep(1);
	}
	return -ETIMEDOUT;
}

/* 写状态寄存器1 */
static int w25q64_write_status_reg1(uint8_t status_value)
{
	int ret = w25q64_write_enable();
	if (ret != 0) {
		return ret;
	}
	
	k_msleep(1);
	
	uint8_t cmd[2];
	cmd[0] = W25Q64_CMD_WRITE_STATUS_REG1;
	cmd[1] = status_value;
	
	const struct spi_buf tx_buf = {
		.buf = cmd,
		.len = 2,
	};
	const struct spi_buf_set tx_bufs = {
		.buffers = &tx_buf,
		.count = 1,
	};
	
	w25q64_cs_select();
	ret = spi_write(w25q64_spi_dev, &w25q64_spi_cfg, &tx_bufs);
	w25q64_cs_deselect();
	if (ret != 0) {
		return ret;
	}
	
	return w25q64_wait_ready();
}

/* 清除块保护 */
int w25q64_clear_block_protect(void)
{
	uint8_t status;
	int ret = w25q64_read_status_reg1(&status);
	if (ret != 0) {
		return ret;
	}
	
	uint8_t bp = (status >> 2) & 0x07;
	if (bp == 0) {
		printk("[W25Q64] 块保护未启用，无需清除\r\n");
		return 0;
	}
	
	uint8_t new_status = status & 0xE3;
	ret = w25q64_write_status_reg1(new_status);
	if (ret != 0) {
		printk("[W25Q64] 清除块保护失败: %d\r\n", ret);
		return ret;
	}
	
	k_msleep(10);
	ret = w25q64_read_status_reg1(&status);
	if (ret == 0) {
		uint8_t new_bp = (status >> 2) & 0x07;
		if (new_bp == 0) {
			printk("[W25Q64] 块保护清除成功\r\n");
		} else {
			printk("[W25Q64] 警告: 块保护清除后仍为 BP=%d\r\n", new_bp);
			return -EIO;
		}
	}
	
	return 0;
}

/* 检查写保护状态 */
int w25q64_check_write_protect(void)
{
	uint8_t status;
	int ret = w25q64_read_status_reg1(&status);
	if (ret != 0) {
		return ret;
	}
	
	bool busy = (status & 0x01) != 0;
	bool wel = (status & 0x02) != 0;
	uint8_t bp = (status >> 2) & 0x07;
	
	printk("[W25Q64] 状态: BUSY=%d, WEL=%d, BP=%d\r\n", busy, wel, bp);
	
	if (bp != 0) {
		printk("[W25Q64] 警告: 块保护已启用 (BP=%d)\r\n", bp);
	}
	
	return 0;
}

/* 写使能 */
int w25q64_write_enable(void)
{
	int ret;
	uint8_t status;
	const int max_retries = 3;
	
	for (int attempt = 0; attempt < max_retries; attempt++) {
		uint8_t cmd = W25Q64_CMD_WRITE_ENABLE;
		const struct spi_buf tx_buf = {
			.buf = &cmd,
			.len = 1,
		};
		const struct spi_buf_set tx_bufs = {
			.buffers = &tx_buf,
			.count = 1,
		};
		
		w25q64_cs_select();
		ret = spi_write(w25q64_spi_dev, &w25q64_spi_cfg, &tx_bufs);
		w25q64_cs_deselect();
		
		if (ret != 0) {
			if (attempt < max_retries - 1) {
				k_msleep(10);
			}
			continue;
		}
		
		k_msleep(1);
		
		ret = w25q64_read_status_reg1(&status);
		if (ret == 0 && (status & 0x02) != 0) {
			return 0;
		}
		
		if (ret == 0) {
			uint8_t bp = (status >> 2) & 0x07;
			if (bp != 0 && attempt == 0) {
				w25q64_clear_block_protect();
				continue;
			}
		}
		
		if (attempt < max_retries - 1) {
			k_msleep(10);
		}
	}
	
	printk("[W25Q64] 错误: 写使能失败\r\n");
	return -EIO;
}

/* 读取数据 */
int w25q64_read(uint32_t addr, uint8_t *data, size_t len)
{
	static uint8_t tx_buf_data[4 + 256];
	static uint8_t rx_buf_data[4 + 256];
	
	if (len > 256) {
		return -EINVAL;
	}
	
	tx_buf_data[0] = W25Q64_CMD_READ_DATA;
	tx_buf_data[1] = (addr >> 16) & 0xFF;
	tx_buf_data[2] = (addr >> 8) & 0xFF;
	tx_buf_data[3] = addr & 0xFF;
	memset(&tx_buf_data[4], 0xFF, len);

	const struct spi_buf tx_buf = {
		.buf = tx_buf_data,
		.len = 4 + len,
	};
	const struct spi_buf_set tx_bufs = {
		.buffers = &tx_buf,
		.count = 1,
	};

	struct spi_buf rx_buf = {
		.buf = rx_buf_data,
		.len = 4 + len,
	};
	const struct spi_buf_set rx_bufs = {
		.buffers = &rx_buf,
		.count = 1,
	};

	w25q64_cs_select();
	int ret = spi_transceive(w25q64_spi_dev, &w25q64_spi_cfg, &tx_bufs, &rx_bufs);
	w25q64_cs_deselect();
	
	if (ret == 0) {
		memcpy(data, &rx_buf_data[4], len);
	}
	return ret;
}

/* 页编程 */
int w25q64_page_program(uint32_t addr, const uint8_t *data, size_t len)
{
	if (len > W25Q64_PAGE_SIZE) {
		return -EINVAL;
	}
	if ((addr % W25Q64_PAGE_SIZE) + len > W25Q64_PAGE_SIZE) {
		return -EINVAL;
	}

	int ret = w25q64_write_enable();
	if (ret != 0) {
		return ret;
	}

	ret = w25q64_wait_ready();
	if (ret != 0) {
		return ret;
	}

	uint8_t cmd[4];
	cmd[0] = W25Q64_CMD_PAGE_PROGRAM;
	cmd[1] = (addr >> 16) & 0xFF;
	cmd[2] = (addr >> 8) & 0xFF;
	cmd[3] = addr & 0xFF;

	struct spi_buf tx_bufs[2];
	tx_bufs[0].buf = cmd;
	tx_bufs[0].len = 4;
	tx_bufs[1].buf = (uint8_t *)data;
	tx_bufs[1].len = len;

	const struct spi_buf_set tx_set = {
		.buffers = tx_bufs,
		.count = 2,
	};

	w25q64_cs_select();
	ret = spi_write(w25q64_spi_dev, &w25q64_spi_cfg, &tx_set);
	w25q64_cs_deselect();
	if (ret != 0) {
		return ret;
	}

	return w25q64_wait_ready();
}

/* 扇区擦除 */
int w25q64_sector_erase(uint32_t addr)
{
	uint32_t aligned_addr = (addr / W25Q64_SECTOR_SIZE) * W25Q64_SECTOR_SIZE;
	if (aligned_addr != addr) {
		printk("[W25Q64] 警告: 地址 0x%06X 未对齐，修正为 0x%06X\r\n", addr, aligned_addr);
		addr = aligned_addr;
	}

	w25q64_check_write_protect();
	
	int ret = w25q64_write_enable();
	if (ret != 0) {
		return ret;
	}
	
	uint8_t status;
	ret = w25q64_read_status_reg1(&status);
	if (ret == 0 && (status & 0x02) == 0) {
		w25q64_clear_block_protect();
		ret = w25q64_write_enable();
		if (ret != 0) {
			return ret;
		}
	}

	ret = w25q64_wait_ready();
	if (ret != 0) {
		return ret;
	}

	printk("[W25Q64] 发送扇区擦除命令 (地址: 0x%06X)\r\n", addr);
	uint8_t cmd[4];
	cmd[0] = W25Q64_CMD_SECTOR_ERASE;
	cmd[1] = (addr >> 16) & 0xFF;
	cmd[2] = (addr >> 8) & 0xFF;
	cmd[3] = addr & 0xFF;

	const struct spi_buf tx_buf = {
		.buf = cmd,
		.len = 4,
	};
	const struct spi_buf_set tx_bufs = {
		.buffers = &tx_buf,
		.count = 1,
	};

	w25q64_cs_select();
	ret = spi_write(w25q64_spi_dev, &w25q64_spi_cfg, &tx_bufs);
	w25q64_cs_deselect();
	if (ret != 0) {
		return ret;
	}

	ret = w25q64_wait_ready();
	if (ret != 0) {
		printk("[W25Q64] 扇区擦除超时 (地址: 0x%06X)\r\n", addr);
		return ret;
	}
	
	k_msleep(100);
	printk("[W25Q64] 扇区擦除完成 (地址: 0x%06X)\r\n", addr);
	
	return 0;
}

/* 进入深度掉电模式 */
void w25q64_sleep(void)
{
	if (w25q64_in_sleep) {
		return;
	}
	
	if (w25q64_spi_dev == NULL || !device_is_ready(w25q64_spi_dev)) {
		return;
	}
	
	uint8_t cmd = W25Q64_CMD_POWER_DOWN;
	const struct spi_buf tx_buf = {
		.buf = &cmd,
		.len = 1,
	};
	const struct spi_buf_set tx_bufs = {
		.buffers = &tx_buf,
		.count = 1,
	};
	
	w25q64_cs_select();
	int ret = spi_write(w25q64_spi_dev, &w25q64_spi_cfg, &tx_bufs);
	w25q64_cs_deselect();
	
	if (ret == 0) {
		w25q64_in_sleep = true;
		printk("[W25Q64] 进入深度掉电模式 (< 1uA)\r\n");
	}
}

/* 从深度掉电模式唤醒 */
void w25q64_wakeup(void)
{
	if (!w25q64_in_sleep) {
		return;
	}
	
	if (w25q64_spi_dev == NULL || !device_is_ready(w25q64_spi_dev)) {
		return;
	}
	
	uint8_t cmd = W25Q64_CMD_RELEASE_POWER_DOWN;
	const struct spi_buf tx_buf = {
		.buf = &cmd,
		.len = 1,
	};
	const struct spi_buf_set tx_bufs = {
		.buffers = &tx_buf,
		.count = 1,
	};
	
	w25q64_cs_select();
	int ret = spi_write(w25q64_spi_dev, &w25q64_spi_cfg, &tx_bufs);
	w25q64_cs_deselect();
	
	if (ret != 0) {
		return;
	}
	
	/* 深度掉电唤醒后，芯片需 3µs~3ms 才能稳定接受写使能等命令，留足余量避免写使能失败 */
	k_busy_wait(3000);
	
	uint8_t status;
	int status_retry = 0;
	const int max_status_retries = 20;
	
	while (status_retry < max_status_retries) {
		ret = w25q64_read_status_reg1(&status);
		if (ret == 0 && (status & 0x01) == 0) {
			break;
		}
		k_busy_wait(10);
		status_retry++;
	}
	
	w25q64_in_sleep = false;
	printk("[W25Q64] 已从深度掉电模式唤醒\r\n");
}

/* 初始化 W25Q64 Flash：SPI 由 alias w25q64-spi 指定；52832 上若 alias 指向被禁用的 spi0 则回退到 spi1 */
int w25q64_init(void)
{
#if DT_NODE_EXISTS(DT_ALIAS(w25q64_spi)) && DT_NODE_HAS_STATUS(DT_ALIAS(w25q64_spi), okay)
	w25q64_spi_dev = DEVICE_DT_GET(DT_ALIAS(w25q64_spi));
#else
	w25q64_spi_dev = NULL;
#endif
#if defined(CONFIG_SOC_NRF52832)
	if (w25q64_spi_dev == NULL && DT_NODE_EXISTS(DT_NODELABEL(spi1)) && DT_NODE_HAS_STATUS(DT_NODELABEL(spi1), okay)) {
		w25q64_spi_dev = DEVICE_DT_GET(DT_NODELABEL(spi1));
	}
#endif
	if (w25q64_spi_dev == NULL) {
		printk("[W25Q64] 错误: SPI设备指针为NULL\r\n");
#if defined(CONFIG_SOC_NRF52832)
		printk("[W25Q64] 52832 需启用 spi1：构建时加上 -- -DDTC_OVERLAY_FILE=boards/nrf52832dk_nrf52832_cpuapp.overlay\r\n");
#endif
		return -ENODEV;
	}
	
	printk("[W25Q64] SPI设备名称: %s\r\n", w25q64_spi_dev->name);
	
	if (!device_is_ready(w25q64_spi_dev)) {
		printk("[W25Q64] SPI设备未就绪\r\n");
		return -ENODEV;
	}
	
	printk("[W25Q64] SPI设备已就绪\r\n");
	
	int ret = w25q64_wait_ready();
	if (ret != 0) {
		printk("[W25Q64] 等待Flash就绪失败: %d\n", ret);
		return ret;
	}
	
	w25q64_check_write_protect();

	const struct device *gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
	if (!device_is_ready(gpio_dev)) {
		printk("[W25Q64] GPIO设备未就绪\n");
		return -ENODEV;
	}
	
	ret = gpio_pin_configure(gpio_dev, W25Q64_CS_PIN, GPIO_OUTPUT_INACTIVE);
	if (ret != 0) {
		printk("[W25Q64] CS GPIO配置失败: %d\n", ret);
		return ret;
	}
	
	gpio_pin_set(gpio_dev, W25Q64_CS_PIN, 1);

	/* 读取JEDEC ID验证芯片 */
	uint8_t tx_data[4] = {W25Q64_CMD_JEDEC_ID, 0xFF, 0xFF, 0xFF};
	uint8_t rx_data[4] = {0};
	uint8_t id[3];
	const struct spi_buf tx_buf = {
		.buf = tx_data,
		.len = 4,
	};
	const struct spi_buf_set tx_bufs = {
		.buffers = &tx_buf,
		.count = 1,
	};
	struct spi_buf rx_buf = {
		.buf = rx_data,
		.len = 4,
	};
	const struct spi_buf_set rx_bufs = {
		.buffers = &rx_buf,
		.count = 1,
	};

	w25q64_cs_select();
	ret = spi_transceive(w25q64_spi_dev, &w25q64_spi_cfg, &tx_bufs, &rx_bufs);
	w25q64_cs_deselect();
	
	id[0] = rx_data[1];
	id[1] = rx_data[2];
	id[2] = rx_data[3];
	if (ret != 0) {
		printk("[W25Q64] 读取JEDEC ID失败: %d\r\n", ret);
		return ret;
	}

	printk("[W25Q64] JEDEC ID: 0x%02X 0x%02X 0x%02X\r\n", id[0], id[1], id[2]);
	if (id[0] == 0xEF && id[1] == 0x40 && id[2] == 0x17) {
		printk("[W25Q64] JEDEC ID验证成功\r\n");
	} else {
		printk("[W25Q64] 警告: JEDEC ID不匹配（期望: EF 40 17）\r\n");
	}
	
	printk("[W25Q64] 检查块保护状态...\r\n");
	ret = w25q64_clear_block_protect();
	if (ret != 0) {
		printk("[W25Q64] 警告: 清除块保护失败: %d\r\n", ret);
	}

	printk("[W25Q64] 初始化完成\r\n");
	
	w25q64_sleep();
	
	return 0;
}
