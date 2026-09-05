/**
 * @file nvs_config.c
 * @brief NVS 配置管理
 */

#include "nvs_config.h"
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/devicetree.h>

/* NVS 文件系统 */
static struct nvs_fs nvs_fs;
static bool nvs_ready = false;
static const struct device *flash_dev;

/* 获取 NVS 文件系统指针 */
struct nvs_fs *nvs_get_fs(void)
{
	return &nvs_fs;
}

/* 检查 NVS 是否就绪 */
bool nvs_config_is_ready(void)
{
	return nvs_ready;
}

/* NVS 分区：按 SoC 写死，避免 overlay 合并顺序导致 52832 误用 52810 的 0x2A000 而 -45 */
#if defined(CONFIG_SOC_NRF52832)
#define NVS_PARTITION_OFFSET  0x00078000U
#define NVS_PARTITION_SIZE   (12 * 1024U)
#else
#define NVS_PARTITION_OFFSET  0x0002A000U
#define NVS_PARTITION_SIZE   (12 * 1024U)
#endif

/* 初始化 NVS */
int nvs_config_init(void)
{
	flash_dev = DEVICE_DT_GET(DT_NODELABEL(flash_controller));
	if (flash_dev == NULL || !device_is_ready(flash_dev)) {
		printk("[NVS] Flash 未就绪，NVS 不可用\r\n");
		return -1;
	}
	nvs_fs.flash_device = flash_dev;
	nvs_fs.offset = NVS_PARTITION_OFFSET;
	nvs_fs.sector_size = NVS_SECTOR_SIZE;
	nvs_fs.sector_count = NVS_PARTITION_SIZE / NVS_SECTOR_SIZE;
	int ret = nvs_mount(&nvs_fs);
	if (ret != 0) {
		printk("[NVS] 挂载失败: %d (offset=0x%X size=%u)\r\n",
		       ret, (unsigned)NVS_PARTITION_OFFSET, (unsigned)NVS_PARTITION_SIZE);
		return ret;
	}
	nvs_ready = true;
	printk("[NVS] 已挂载 (offset=0x%X, size=%u)\r\n",
	       (unsigned)NVS_PARTITION_OFFSET, (unsigned)NVS_PARTITION_SIZE);
	return 0;
}

/* 从 NVS 加载存储位置 */
void nvs_load_storage_position(storage_position_t *pos)
{
	if (!nvs_ready || pos == NULL) {
		return;
	}
	uint16_t v;
	ssize_t len;
	bool any = false;
	
	len = nvs_read(&nvs_fs, NVS_ID_NEXT_SECTOR, &v, sizeof(v));
	if (len == sizeof(v)) {
		pos->next_sector = v;
		any = true;
	}
	len = nvs_read(&nvs_fs, NVS_ID_OLDEST_SECTOR, &v, sizeof(v));
	if (len == sizeof(v)) {
		pos->oldest_sector = v;
		any = true;
	}
	len = nvs_read(&nvs_fs, NVS_ID_NEXT_RECORD, &v, sizeof(v));
	if (len == sizeof(v)) {
		pos->next_record_in_sector = v;
		any = true;
	}
	if (any) {
		printk("[NVS] 位置已加载 next=%d oldest=%d rec=%d\r\n",
		       pos->next_sector, pos->oldest_sector, pos->next_record_in_sector);
	} else {
		printk("[NVS] 无位置缓存，使用默认值\r\n");
	}
}

/* 保存存储位置到 NVS */
void nvs_save_storage_position(const storage_position_t *pos)
{
	if (!nvs_ready || pos == NULL) {
		return;
	}
	int ret;
	ret = nvs_write(&nvs_fs, NVS_ID_NEXT_SECTOR, &pos->next_sector, sizeof(pos->next_sector));
	if (ret >= 0) {
		ret = nvs_write(&nvs_fs, NVS_ID_OLDEST_SECTOR, &pos->oldest_sector, sizeof(pos->oldest_sector));
	}
	if (ret >= 0) {
		ret = nvs_write(&nvs_fs, NVS_ID_NEXT_RECORD, &pos->next_record_in_sector, sizeof(pos->next_record_in_sector));
	}
	if (ret >= 0) {
		printk("[NVS] 位置已写入 next=%d oldest=%d rec=%d\r\n",
		       pos->next_sector, pos->oldest_sector, pos->next_record_in_sector);
	} else {
		printk("[NVS] 位置写入失败: %d\r\n", ret);
	}
}

/* 清空存储位置信息 */
void nvs_clear_storage_position(void)
{
	if (!nvs_ready) {
		return;
	}
	uint16_t zero = 0;
	int ret;
	ret = nvs_write(&nvs_fs, NVS_ID_NEXT_SECTOR, &zero, sizeof(zero));
	if (ret >= 0) {
		ret = nvs_write(&nvs_fs, NVS_ID_OLDEST_SECTOR, &zero, sizeof(zero));
	}
	if (ret >= 0) {
		ret = nvs_write(&nvs_fs, NVS_ID_NEXT_RECORD, &zero, sizeof(zero));
	}
	if (ret >= 0) {
		printk("[NVS] 位置信息已清空\r\n");
	} else {
		printk("[NVS] 位置信息清空失败: %d\r\n", ret);
	}
}

/* 从 NVS 加载时间基准 */
void nvs_load_time(uint32_t *timestamp, bool *time_synced)
{
	if (!nvs_ready) {
		*time_synced = false;
		*timestamp = 0;
		return;
	}
	uint32_t saved_timestamp;
	ssize_t len = nvs_read(&nvs_fs, NVS_ID_TIME_BASE, &saved_timestamp, sizeof(saved_timestamp));
	if (len == sizeof(saved_timestamp) && saved_timestamp > 946684800) {
		*timestamp = saved_timestamp;
		*time_synced = true;
		
		uint32_t days = saved_timestamp / 86400;
		uint32_t seconds = saved_timestamp % 86400;
		uint32_t hours = seconds / 3600;
		uint32_t minutes = (seconds % 3600) / 60;
		uint32_t secs = seconds % 60;
		
		printk("[NVS] 时间已加载 时间戳: %u\r\n", saved_timestamp);
		printk("[NVS] UTC 时间: 自1970-01-01起 %u 天 %02u:%02u:%02u\r\n",
		       days, hours, minutes, secs);
	} else {
		printk("[NVS] 无时间缓存，等待时间同步\r\n");
		*time_synced = false;
		*timestamp = 0;
	}
}

/* 保存时间到 NVS */
int nvs_save_time(uint32_t timestamp)
{
	if (!nvs_ready) {
		return -1;
	}
	int ret = nvs_write(&nvs_fs, NVS_ID_TIME_BASE, &timestamp, sizeof(timestamp));
	if (ret >= 0) {
		printk("[NVS] 时间已保存: %u\r\n", timestamp);
		return 0;
	}
	printk("[NVS] 时间保存失败: %d\r\n", ret);
	return ret;
}

/* 从 NVS 加载设备配置 */
void nvs_load_config(device_config_ctx_t *config)
{
	/* 设置默认值 */
	config->sample_interval = DEFAULT_SAMPLE_INTERVAL;
	config->max_temperature = INT16_MIN;
	config->min_temperature = INT16_MAX;
	config->max_temperature_timestamp = 0;
	config->min_temperature_timestamp = 0;
	config->firmware_version = FIRMWARE_VERSION;
	config->record_count = 0;
	config->config_dirty = false;
	config->position_dirty = false;

	if (!nvs_ready) {
		printk("[配置] NVS 未就绪，使用默认配置\r\n");
		return;
	}
	
	device_config_t nvs_config;
	ssize_t len = nvs_read(&nvs_fs, NVS_ID_CONFIG, &nvs_config, sizeof(nvs_config));
	
	if (len == 8) {
		printk("[配置] 检测到旧版本配置格式（8字节）\r\n");
	} else if (len == 14) {
		printk("[配置] 检测到中间版本配置格式（14字节）\r\n");
	} else if (len != sizeof(nvs_config)) {
		printk("[配置] 无 NVS 配置，使用默认值\r\n");
		return;
	}
	
	if (nvs_config.sample_interval == 0xFFFF) {
		printk("[配置] NVS 配置未初始化，使用默认值\r\n");
		return;
	}
	
	if (nvs_config.sample_interval >= MIN_SAMPLE_INTERVAL && 
	    nvs_config.sample_interval <= MAX_SAMPLE_INTERVAL) {
		config->sample_interval = nvs_config.sample_interval;
		printk("[配置] 加载成功，采集间隔: %d 秒\r\n", config->sample_interval);
	}
	
	if (nvs_config.max_temperature != 0xFFFF && nvs_config.max_temperature != 0x0000 &&
	    nvs_config.max_temperature >= -5000 && nvs_config.max_temperature <= 10000) {
		config->max_temperature = nvs_config.max_temperature;
		if (len >= 14) {
			config->max_temperature_timestamp = nvs_config.max_temperature_timestamp;
		}
		printk("[配置] 最高温度: %d.%02d°C\r\n",
		       config->max_temperature / 100, 
		       (config->max_temperature >= 0 ? config->max_temperature : -config->max_temperature) % 100);
	}
	
	if (nvs_config.min_temperature != 0xFFFF && nvs_config.min_temperature != 0x0000 &&
	    nvs_config.min_temperature >= -5000 && nvs_config.min_temperature <= 10000) {
		config->min_temperature = nvs_config.min_temperature;
		if (len >= 14) {
			config->min_temperature_timestamp = nvs_config.min_temperature_timestamp;
		}
		printk("[配置] 最低温度: %d.%02d°C\r\n",
		       config->min_temperature / 100,
		       (config->min_temperature >= 0 ? config->min_temperature : -config->min_temperature) % 100);
	}
	
	if (len >= sizeof(nvs_config)) {
		if (nvs_config.firmware_version != 0 && nvs_config.firmware_version != 0xFFFF) {
			config->firmware_version = nvs_config.firmware_version;
		}
		printk("[配置] 固件版本: %d\r\n", config->firmware_version);
	}
}

/* 保存设备配置到 NVS */
void nvs_save_config(device_config_ctx_t *config, bool immediate)
{
	config->config_dirty = true;
	if (immediate) {
		nvs_flush(config, NULL);
	}
}

/* 刷新 NVS */
void nvs_flush(device_config_ctx_t *config, const storage_position_t *pos)
{
	if (!nvs_ready) {
		return;
	}
	
	if (config != NULL && config->config_dirty) {
		device_config_t nvs_config = {
			.sample_interval = config->sample_interval,
			.max_temperature = config->max_temperature,
			.min_temperature = config->min_temperature,
			.max_temperature_timestamp = config->max_temperature_timestamp,
			.min_temperature_timestamp = config->min_temperature_timestamp,
			.firmware_version = config->firmware_version
		};
		int ret = nvs_write(&nvs_fs, NVS_ID_CONFIG, &nvs_config, sizeof(nvs_config));
		if (ret >= 0) {
			config->config_dirty = false;
			printk("[NVS] 配置已写入\r\n");
		} else {
			printk("[NVS] 配置写入失败: %d\r\n", ret);
		}
	}
	
	if (pos != NULL && config != NULL && config->position_dirty) {
		nvs_save_storage_position(pos);
		config->position_dirty = false;
	}
}
