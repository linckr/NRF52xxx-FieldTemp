/**
 * @file nvs_config.h
 * @brief NVS 配置管理头文件
 */

#ifndef STORAGE_NVS_CONFIG_H_
#define STORAGE_NVS_CONFIG_H_

#include <zephyr/kernel.h>
#include <zephyr/fs/nvs.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

/* NVS 分区：offset/size 由 DTS 提供（app.overlay 与 board overlay 中 nvs_partition，52810=0x2A000/12KB，52832=0x78000/12KB）*/
#define NVS_SECTOR_SIZE       4096

/* NVS key 定义 */
#define NVS_ID_NEXT_SECTOR    1
#define NVS_ID_OLDEST_SECTOR  2
#define NVS_ID_NEXT_RECORD    3
#define NVS_ID_CONFIG         4
#define NVS_ID_TIME_BASE      5

/* 配置默认值 */
#define FIRMWARE_VERSION         2
#define DEFAULT_SAMPLE_INTERVAL  1
#define MIN_SAMPLE_INTERVAL      1
#define MAX_SAMPLE_INTERVAL      3600

/* 配置数据结构（存储在Flash）*/
typedef struct {
	uint16_t sample_interval;            /* 采集间隔（秒）*/
	int16_t  max_temperature;            /* 最高温度（0.01°C）*/
	int16_t  min_temperature;            /* 最低温度（0.01°C）*/
	uint32_t max_temperature_timestamp;  /* 最高温度发生时间 */
	uint32_t min_temperature_timestamp;  /* 最低温度发生时间 */
	uint16_t firmware_version;           /* 固件版本号 */
} __packed device_config_t;

/* 设备配置上下文 */
typedef struct {
	uint16_t sample_interval;
	int16_t  max_temperature;
	int16_t  min_temperature;
	uint32_t max_temperature_timestamp;
	uint32_t min_temperature_timestamp;
	uint16_t firmware_version;
	uint16_t record_count;
	bool     config_dirty;
	bool     position_dirty;
} device_config_ctx_t;

/* 存储位置上下文 */
typedef struct {
	uint16_t next_sector;
	uint16_t oldest_sector;
	uint16_t next_record_in_sector;
} storage_position_t;

/**
 * @brief 初始化 NVS
 * @return 0 成功，负数错误码
 */
int nvs_config_init(void);

/**
 * @brief 检查 NVS 是否就绪
 * @return true 已就绪，false 未就绪
 */
bool nvs_config_is_ready(void);

/**
 * @brief 从 NVS 加载存储位置
 * @param pos 存储位置输出
 */
void nvs_load_storage_position(storage_position_t *pos);

/**
 * @brief 保存存储位置到 NVS
 * @param pos 存储位置
 */
void nvs_save_storage_position(const storage_position_t *pos);

/**
 * @brief 清空存储位置信息
 */
void nvs_clear_storage_position(void);

/**
 * @brief 从 NVS 加载时间基准
 * @param timestamp 时间戳输出
 * @param time_synced 时间同步状态输出
 */
void nvs_load_time(uint32_t *timestamp, bool *time_synced);

/**
 * @brief 保存时间到 NVS
 * @param timestamp 时间戳
 * @return 0 成功，负数错误码
 */
int nvs_save_time(uint32_t timestamp);

/**
 * @brief 从 NVS 加载设备配置
 * @param config 配置输出
 */
void nvs_load_config(device_config_ctx_t *config);

/**
 * @brief 保存设备配置到 NVS
 * @param config 配置
 * @param immediate true 立即保存，false 延迟保存
 */
void nvs_save_config(device_config_ctx_t *config, bool immediate);

/**
 * @brief 刷新 NVS（保存脏数据）
 * @param config 配置上下文
 * @param pos 存储位置
 */
void nvs_flush(device_config_ctx_t *config, const storage_position_t *pos);

/**
 * @brief 获取 NVS 文件系统指针
 * @return NVS 文件系统指针
 */
struct nvs_fs *nvs_get_fs(void);

#ifdef __cplusplus
}
#endif

#endif /* STORAGE_NVS_CONFIG_H_ */
