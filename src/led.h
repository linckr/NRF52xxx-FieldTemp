/**
 * @file led.h
 * @brief LED 指示灯控制头文件
 * 
 * LED 为低电平有效（低电平点亮，高电平熄灭）
 */

#ifndef LED_H_
#define LED_H_

#include <zephyr/kernel.h>
#include <hal/nrf_gpio.h>
#include "board_pins.h"

#ifdef __cplusplus
extern "C" {
#endif

/* LED 引脚定义（引用 board_pins.h）*/
#define DATA_LED_PIN       PIN_LED_DATA
#define LINK_LED_PIN       PIN_LED_LINK

#define LED_PORT           NRF_P0
#define DATA_LED_MASK      (1UL << DATA_LED_PIN)
#define LINK_LED_MASK      (1UL << LINK_LED_PIN)
#define LED_MASK           (DATA_LED_MASK | LINK_LED_MASK)

/* 闪烁间隔定义 */
#define INIT_BLINK_MS      200  /* 初始化时闪烁间隔200毫秒 */
#define LINK_BLINK_MS      500  /* LINK灯闪烁间隔500毫秒 */
#define DATA_BLINK_MS      100  /* DATA灯闪烁间隔100毫秒（时间同步指示）*/

/**
 * @brief 初始化 LED GPIO
 */
void led_init(void);

/**
 * @brief 点亮 LED（低电平有效）
 * @param mask LED 掩码
 */
static inline void led_on(uint32_t mask)
{
	LED_PORT->OUTCLR = mask;
}

/**
 * @brief 熄灭 LED（高电平熄灭）
 * @param mask LED 掩码
 */
static inline void led_off(uint32_t mask)
{
	LED_PORT->OUTSET = mask;
}

/**
 * @brief 翻转 LED 状态
 * @param mask LED 掩码
 */
static inline void led_toggle(uint32_t mask)
{
	LED_PORT->OUT ^= mask;
}

/**
 * @brief 启动初始化闪烁（两个灯都闪烁）
 */
void led_start_init_blink(void);

/**
 * @brief 停止初始化闪烁
 */
void led_stop_init_blink(void);

/**
 * @brief 启动 LINK 灯闪烁
 */
void led_start_link_blink(void);

/**
 * @brief 停止 LINK 灯闪烁
 */
void led_stop_link_blink(void);

/**
 * @brief 启动 DATA 灯闪烁（时间同步指示，闪烁3次）
 */
void led_start_data_blink(void);

/**
 * @brief DATA LED 短暂点亮指示（数据更新）
 */
void led_data_flash(void);

/**
 * @brief 设置蓝牙就绪状态
 * @param ready 是否就绪
 */
void led_set_bt_ready(bool ready);

/**
 * @brief 设置蓝牙连接状态
 * @param connected 是否已连接
 */
void led_set_bt_connected(bool connected);

#ifdef __cplusplus
}
#endif

#endif /* LED_H_ */
