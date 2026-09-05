/**
 * @file ble_adv.c
 * @brief 蓝牙广播管理
 */

#include "ble_adv.h"
#include <zephyr/sys/printk.h>
#include <stdio.h>
#include <string.h>

/* 广播数据 */
static const uint8_t ad_flags[] = { BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR };
static char device_name_with_mac[DEVICE_NAME_MAX_LEN];
static uint8_t device_name_len = 0;
static struct bt_data ad[2];

/* 状态标志 */
static bool bt_connected_flag = false;
static bool bt_ready_flag = false;
static bool intermittent_adv_enabled = false;

/* 工作队列 */
static struct k_work_delayable adv_restart_work;
static struct k_work_delayable adv_slow_work;
static struct k_work_delayable adv_start_work;
static struct k_work_delayable adv_stop_work;

/* 前向声明 */
static void adv_restart_work_handler(struct k_work *work);
static void adv_slow_work_handler(struct k_work *work);
static void adv_start_work_handler(struct k_work *work);
static void adv_stop_work_handler(struct k_work *work);

/* 获取设备名称 */
const char *ble_adv_get_device_name(void)
{
	return device_name_with_mac;
}

/* 设置蓝牙连接状态 */
void ble_adv_set_connected(bool connected)
{
	bt_connected_flag = connected;
}

/* 初始化蓝牙广播 */
int ble_adv_init(void)
{
	/* 初始化工作队列 */
	k_work_init_delayable(&adv_restart_work, adv_restart_work_handler);
	k_work_init_delayable(&adv_slow_work, adv_slow_work_handler);
	k_work_init_delayable(&adv_start_work, adv_start_work_handler);
	k_work_init_delayable(&adv_stop_work, adv_stop_work_handler);

	/* 获取蓝牙MAC地址并构建动态设备名称 */
	bt_addr_le_t addrs[1];
	size_t count = 1;
	bt_id_get(addrs, &count);
	
	snprintf(device_name_with_mac, sizeof(device_name_with_mac),
		"%s_%02X%02X", DEVICE_NAME_BASE,
		addrs[0].a.val[1], addrs[0].a.val[0]);
	device_name_len = strlen(device_name_with_mac);
	
	/* 设置动态蓝牙名称 */
	int err = bt_set_name(device_name_with_mac);
	if (err) {
		printk("[BLE广播] 设置蓝牙名称失败: %d\r\n", err);
	}
	
	/* 构建广播数据 */
	ad[0].type = BT_DATA_FLAGS;
	ad[0].data_len = sizeof(ad_flags);
	ad[0].data = ad_flags;
	
	ad[1].type = BT_DATA_NAME_COMPLETE;
	ad[1].data_len = device_name_len;
	ad[1].data = (const uint8_t *)device_name_with_mac;

	printk("[BLE广播] 初始化完成，设备名称: %s\r\n", device_name_with_mac);
	printk("[BLE广播] MAC地址: %02X:%02X:%02X:%02X:%02X:%02X\r\n",
		addrs[0].a.val[5], addrs[0].a.val[4], addrs[0].a.val[3],
		addrs[0].a.val[2], addrs[0].a.val[1], addrs[0].a.val[0]);
	
	bt_ready_flag = true;
	return 0;
}

/* 快速广播参数（不使用 BT_LE_ADV_OPT_USE_NAME，避免与手动设置名称冲突） */
static struct bt_le_adv_param fast_adv_param = BT_LE_ADV_PARAM_INIT(
	BT_LE_ADV_OPT_CONN,  /* 仅可连接，不自动添加名称 */
	BT_GAP_ADV_FAST_INT_MIN_1,  /* 100ms */
	BT_GAP_ADV_FAST_INT_MAX_1,  /* 150ms */
	NULL
);

/* 启动快速广播 */
int ble_adv_start_fast(void)
{
	int err = bt_le_adv_start(&fast_adv_param, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		printk("[BLE广播] 快速广播启动失败: %d\r\n", err);
		return err;
	}
	printk("[BLE广播] 快速广播已启动，名称: %s\r\n", device_name_with_mac);
	return 0;
}

/* 启动慢速广播 */
int ble_adv_start_slow(void)
{
	struct bt_le_adv_param param = {
		.id = BT_ID_DEFAULT,
		.options = BT_LE_ADV_OPT_CONN,  /* 仅可连接，不自动添加名称 */
		.interval_min = 0x800,   /* 1.28秒 */
		.interval_max = 0x1000,  /* 2.56秒 */
		.peer = NULL,
	};
	
	int err = bt_le_adv_start(&param, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		printk("[BLE广播] 慢速广播启动失败: %d\r\n", err);
		return err;
	}
	printk("[BLE广播] 慢速广播已启动，名称: %s\r\n", device_name_with_mac);
	return 0;
}

/* 停止广播 */
int ble_adv_stop(void)
{
	int ret = bt_le_adv_stop();
	if (ret != 0 && ret != -EALREADY) {
		printk("[BLE广播] 停止广播失败: %d\r\n", ret);
		return ret;
	}
	return 0;
}

/* 快速广播重启工作函数 */
static void adv_restart_work_handler(struct k_work *work)
{
	if (bt_connected_flag) {
		return;
	}

	int err = ble_adv_start_fast();
	if (err) {
		k_work_schedule(&adv_restart_work, K_MSEC(500));
	} else {
		k_work_cancel_delayable(&adv_slow_work);
		k_work_schedule(&adv_slow_work, K_SECONDS(30));
	}
}

/* 慢速广播工作函数 */
static void adv_slow_work_handler(struct k_work *work)
{
	if (bt_connected_flag) {
		return;
	}
	
	ble_adv_stop();
	k_msleep(50);
	
	int err = ble_adv_start_slow();
	if (err) {
		ble_adv_start_fast();
	} else {
		ble_adv_start_intermittent();
	}
}

/* 间歇广播启动工作函数 */
static void adv_start_work_handler(struct k_work *work)
{
	if (bt_connected_flag) {
		printk("[BLE广播] 已连接，停止间歇广播\r\n");
		intermittent_adv_enabled = false;
		return;
	}
	
	if (!intermittent_adv_enabled) {
		return;
	}
	
	ble_adv_stop();
	k_msleep(50);
	
	int err = ble_adv_start_slow();
	if (err) {
		k_work_schedule(&adv_start_work, K_MSEC(200));
	} else {
		printk("[BLE广播] 间歇广播：已启动（持续10秒）\r\n");
		k_work_schedule(&adv_stop_work, K_SECONDS(10));
	}
}

/* 间歇广播停止工作函数 */
static void adv_stop_work_handler(struct k_work *work)
{
	if (bt_connected_flag) {
		printk("[BLE广播] 已连接，保持连接状态\r\n");
		intermittent_adv_enabled = false;
		return;
	}
	
	if (!intermittent_adv_enabled) {
		return;
	}
	
	int ret = ble_adv_stop();
	if (ret == 0 || ret == -EALREADY) {
		printk("[BLE广播] 间歇广播：已停止（休眠10秒）\r\n");
	}
	
	k_work_schedule(&adv_start_work, K_SECONDS(10));
}

/* 启动间歇广播模式 */
void ble_adv_start_intermittent(void)
{
	if (!bt_ready_flag) {
		return;
	}
	
	k_work_cancel_delayable(&adv_slow_work);
	k_work_cancel_delayable(&adv_restart_work);
	
	intermittent_adv_enabled = true;
	printk("[BLE广播] 启动间歇广播模式\r\n");
	
	k_work_schedule(&adv_start_work, K_NO_WAIT);
}

/* 停止间歇广播模式 */
void ble_adv_stop_intermittent(void)
{
	intermittent_adv_enabled = false;
	k_work_cancel_delayable(&adv_start_work);
	k_work_cancel_delayable(&adv_stop_work);
	printk("[BLE广播] 停止间歇广播模式\r\n");
}

/* 调度广播重启 */
void ble_adv_schedule_restart(void)
{
	k_work_schedule(&adv_restart_work, K_MSEC(100));
}

/* 调度慢速广播切换 */
void ble_adv_schedule_slow(uint32_t delay_sec)
{
	k_work_cancel_delayable(&adv_slow_work);
	k_work_schedule(&adv_slow_work, K_SECONDS(delay_sec));
}
