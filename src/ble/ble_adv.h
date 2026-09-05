/**
 * @file ble_adv.h
 * @brief 蓝牙广播管理头文件
 */

#ifndef BLE_BLE_ADV_H_
#define BLE_BLE_ADV_H_

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 设备名称配置 */
#define DEVICE_NAME_BASE "PandaTemp"
#define DEVICE_NAME_MAX_LEN 24

/**
 * @brief 初始化蓝牙广播
 * @return 0 成功，负数错误码
 */
int ble_adv_init(void);

/**
 * @brief 启动快速广播
 * @return 0 成功，负数错误码
 */
int ble_adv_start_fast(void);

/**
 * @brief 启动慢速广播
 * @return 0 成功，负数错误码
 */
int ble_adv_start_slow(void);

/**
 * @brief 停止广播
 * @return 0 成功，负数错误码
 */
int ble_adv_stop(void);

/**
 * @brief 启动间歇广播模式
 */
void ble_adv_start_intermittent(void);

/**
 * @brief 停止间歇广播模式
 */
void ble_adv_stop_intermittent(void);

/**
 * @brief 设置蓝牙连接状态（用于广播控制）
 * @param connected 是否已连接
 */
void ble_adv_set_connected(bool connected);

/**
 * @brief 调度广播重启
 */
void ble_adv_schedule_restart(void);

/**
 * @brief 调度慢速广播切换
 * @param delay_sec 延迟秒数
 */
void ble_adv_schedule_slow(uint32_t delay_sec);

/**
 * @brief 获取设备名称
 * @return 设备名称字符串
 */
const char *ble_adv_get_device_name(void);

#ifdef __cplusplus
}
#endif

#endif /* BLE_BLE_ADV_H_ */
