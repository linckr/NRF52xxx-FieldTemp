/**
 * @file led.c
 * @brief LED 指示灯控制
 */

#include "led.h"
#include <zephyr/sys/printk.h>

/* 工作队列声明 */
static struct k_work_delayable init_blink_work;
static struct k_work_delayable link_blink_work;
static struct k_work_delayable data_blink_work;
static struct k_work_delayable data_led_off_work;

/* 蓝牙状态标志（由外部设置）*/
static bool bt_ready_flag = false;
static bool bt_connected_flag = false;

/* DATA灯闪烁计数 */
static int data_blink_count = 0;

/* 初始化闪烁工作函数（低功耗模式：不再闪烁）*/
static void init_blink_work_handler(struct k_work *work)
{
	/* 低功耗优化：初始化期间不再闪烁LED，保持熄灭 */
	(void)work;
}

/* LINK灯闪烁工作函数（低功耗模式：不再闪烁）*/
static void link_blink_work_handler(struct k_work *work)
{
	/* 低功耗优化：未连接时不再闪烁LED，保持熄灭 */
	/* 只有蓝牙连接时才点亮LINK LED（由 bt_connected_cb 控制）*/
	(void)work;
}

/* DATA灯闪烁工作函数（时间同步指示）*/
static void data_blink_work_handler(struct k_work *work)
{
	data_blink_count++;
	
	/* 闪烁3次后停止 */
	if (data_blink_count < 6) {  /* 3次闪烁 = 6次状态切换 */
		led_toggle(DATA_LED_MASK);
		k_work_schedule(&data_blink_work, K_MSEC(DATA_BLINK_MS));
	} else {
		data_blink_count = 0;
		led_off(DATA_LED_MASK);
	}
}

/* DATA LED熄灭工作函数（用于温湿度数据指示）*/
static void data_led_off_work_handler(struct k_work *work)
{
	led_off(DATA_LED_MASK);
}

/* 初始化 LED GPIO */
void led_init(void)
{
	/* 配置 LED GPIO 为输出模式 */
	nrf_gpio_cfg_output(DATA_LED_PIN);
	nrf_gpio_cfg_output(LINK_LED_PIN);
	
	/* 初始化时所有 LED 熄灭（高电平）*/
	led_off(LED_MASK);
	
	/* 初始化工作队列 */
	k_work_init_delayable(&init_blink_work, init_blink_work_handler);
	k_work_init_delayable(&link_blink_work, link_blink_work_handler);
	k_work_init_delayable(&data_blink_work, data_blink_work_handler);
	k_work_init_delayable(&data_led_off_work, data_led_off_work_handler);
	
	printk("[LED] GPIO 初始化完成 (DATA: P0.%d, LINK: P0.%d)\r\n", 
	       DATA_LED_PIN, LINK_LED_PIN);
}

/* 设置蓝牙就绪状态 */
void led_set_bt_ready(bool ready)
{
	bt_ready_flag = ready;
}

/* 设置蓝牙连接状态 */
void led_set_bt_connected(bool connected)
{
	bt_connected_flag = connected;
}

/* 启动初始化闪烁 */
void led_start_init_blink(void)
{
	k_work_schedule(&init_blink_work, K_MSEC(INIT_BLINK_MS));
}

/* 停止初始化闪烁 */
void led_stop_init_blink(void)
{
	k_work_cancel_delayable(&init_blink_work);
}

/* 启动 LINK 灯闪烁 */
void led_start_link_blink(void)
{
	k_work_schedule(&link_blink_work, K_MSEC(LINK_BLINK_MS));
}

/* 停止 LINK 灯闪烁 */
void led_stop_link_blink(void)
{
	k_work_cancel_delayable(&link_blink_work);
}

/* 启动 DATA 灯闪烁 */
void led_start_data_blink(void)
{
	k_work_cancel_delayable(&data_blink_work);
	led_off(DATA_LED_MASK);  /* 先熄灭 */
	data_blink_count = 0;    /* 重置计数器 */
	k_work_schedule(&data_blink_work, K_MSEC(DATA_BLINK_MS));
}

/* DATA LED 短暂点亮指示 */
void led_data_flash(void)
{
	led_on(DATA_LED_MASK);
	k_work_schedule(&data_led_off_work, K_MSEC(50));  /* 50ms后熄灭 */
}
