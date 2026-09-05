/**
 * @file ble_services.c
 * @brief 蓝牙 GATT 服务定义
 * 
 * 注意：由于 Zephyr GATT 服务宏需要在编译时静态定义，
 * 完整的服务实现仍保留在 main.c 中。
 * 此文件提供历史传输上下文管理函数。
 */

#include "ble_services.h"
#include <zephyr/sys/printk.h>
#include <string.h>

/* 历史数据传输上下文（使用结构体封装优化）*/
static history_transfer_ctx_t history_ctx = {
	.sector = 0,
	.record_idx = 0,
	.start_timestamp = 0,
	.active = false,
	.binary_search_done = false,
	.total_sent_packets = 0,
	.total_sent_records = 0,
	.last_sent_timestamp = 0,
	.sending = false,
};

/* 获取历史传输上下文 */
history_transfer_ctx_t *ble_get_history_ctx(void)
{
	return &history_ctx;
}

/* 重置历史传输上下文 */
void ble_reset_history_ctx(void)
{
	history_ctx.sector = 0;
	history_ctx.record_idx = 0;
	history_ctx.start_timestamp = 0;
	history_ctx.active = false;
	history_ctx.binary_search_done = false;
	history_ctx.total_sent_packets = 0;
	history_ctx.total_sent_records = 0;
	history_ctx.last_sent_timestamp = 0;
	history_ctx.sending = false;
	memset(history_ctx.tx_buf, 0, sizeof(history_ctx.tx_buf));
	
	printk("[BLE服务] 历史传输上下文已重置\r\n");
}

/* 启动历史数据传输 */
void ble_start_history_transfer(uint32_t start_timestamp)
{
	history_ctx.start_timestamp = start_timestamp;
	history_ctx.active = true;
	history_ctx.binary_search_done = false;
	history_ctx.total_sent_packets = 0;
	history_ctx.total_sent_records = 0;
	history_ctx.last_sent_timestamp = 0;
	
	if (start_timestamp == 0) {
		printk("[BLE服务] 开始历史数据传输（全部）\r\n");
	} else {
		printk("[BLE服务] 开始历史数据传输（时间戳 > %u）\r\n", start_timestamp);
	}
}

/* 停止历史数据传输 */
void ble_stop_history_transfer(void)
{
	history_ctx.active = false;
	history_ctx.sending = false;
	
	printk("[BLE服务] 停止历史数据传输 (已发送 %u 包, %u 条记录)\r\n",
	       history_ctx.total_sent_packets, history_ctx.total_sent_records);
}
