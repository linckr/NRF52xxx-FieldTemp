#include "offline_clock.h"
#include "nvs_config.h"
#include "w25q64.h"
#include "ble/ble_ota.h"
#include <zephyr/sys/byteorder.h>
#include <errno.h>
#include <string.h>

#define CLOCK_KEY 8U
#define CLOCK_MAGIC 0x434c4b31U
#define CLOCK_RESERVATION 3600U
/* One atomic NVS object. Reservations are committed before their IDs are used. */
static struct {
    uint32_t magic, high_water, anchor_base, anchor_unix, anchor_uptime, checksum;
} clock_state;
static uint32_t boot_base;

static uint32_t clock_checksum(void)
{
    const uint8_t *bytes = (const uint8_t *)&clock_state;
    uint32_t value = 2166136261U;
    for (unsigned i = 0; i < sizeof(clock_state) - sizeof(uint32_t); ++i)
        value = (value ^ bytes[i]) * 16777619U;
    return value;
}

static int clock_save(void)
{
    clock_state.checksum = clock_checksum();
    return nvs_write(nvs_get_fs(), CLOCK_KEY, &clock_state, sizeof(clock_state));
}

int offline_clock_boot(void)
{
    if (!nvs_config_is_ready()) return -ENODEV;
    k_mutex_lock(w25q64_get_mutex(), K_FOREVER);
    int len = nvs_read(nvs_get_fs(), CLOCK_KEY, &clock_state, sizeof(clock_state));
    int ret = 0;
    if (len == -ENOENT) {
        memset(&clock_state, 0, sizeof(clock_state));
        clock_state.magic = CLOCK_MAGIC;
    } else if (len != sizeof(clock_state) || clock_state.magic != CLOCK_MAGIC ||
               clock_state.high_water >= OFFLINE_TIME_LIMIT || clock_state.checksum != clock_checksum()) {
        ret = -EINVAL;
        goto out;
    }
    if (clock_state.high_water >= OFFLINE_TIME_LIMIT - CLOCK_RESERVATION) {
        ret = -ENOSPC;
        goto out;
    }
    uint32_t next = clock_state.high_water + 1U;
    clock_state.high_water += CLOCK_RESERVATION;
    ret = clock_save();
    if (ret >= 0) { boot_base = next; ret = 0; }
out:
    k_mutex_unlock(w25q64_get_mutex());
    return ret;
}

uint32_t offline_clock_now(uint32_t uptime_s)
{
    if (!boot_base || uptime_s >= OFFLINE_TIME_LIMIT - boot_base) return 0U;
    uint32_t value = boot_base + uptime_s;
    k_mutex_lock(w25q64_get_mutex(), K_FOREVER);
    if (value > clock_state.high_water) {
        if (ble_ota_in_progress()) { value = 0U; goto out; }
        uint32_t previous = clock_state.high_water;
        uint64_t reserve = (uint64_t)value + CLOCK_RESERVATION;
        if (reserve >= OFFLINE_TIME_LIMIT) value = 0U;
        else {
            clock_state.high_water = reserve;
            if (clock_save() < 0) {
                clock_state.high_water = previous;
                value = 0U;
            }
        }
    }
out:
    k_mutex_unlock(w25q64_get_mutex());
    return value;
}

int offline_clock_anchor(uint32_t unix_s, uint32_t uptime_s)
{
    if (!boot_base || unix_s < OFFLINE_TIME_LIMIT || unix_s > 4102444800U ||
            !offline_clock_now(uptime_s)) return -EINVAL;
    k_mutex_lock(w25q64_get_mutex(), K_FOREVER);
    int ret = 0;
    if (clock_state.anchor_base != boot_base) {
        uint32_t old_base = clock_state.anchor_base;
        uint32_t old_unix = clock_state.anchor_unix;
        uint32_t old_uptime = clock_state.anchor_uptime;
        clock_state.anchor_base = boot_base;
        clock_state.anchor_unix = unix_s;
        clock_state.anchor_uptime = uptime_s;
        ret = clock_save();
        if (ret < 0) {
            clock_state.anchor_base = old_base;
            clock_state.anchor_unix = old_unix;
            clock_state.anchor_uptime = old_uptime;
        }
    }
    k_mutex_unlock(w25q64_get_mutex());
    return ret < 0 ? ret : 0;
}

void offline_clock_metadata(uint8_t out[OFFLINE_CLOCK_WIRE_SIZE])
{
    k_mutex_lock(w25q64_get_mutex(), K_FOREVER);
    sys_put_le32(0x31434c4fU, out); /* OLC1 */
    sys_put_le32(boot_base, out + 4);
    sys_put_le32(clock_state.anchor_base, out + 8);
    sys_put_le32(clock_state.anchor_unix, out + 12);
    sys_put_le32(clock_state.anchor_uptime, out + 16);
    sys_put_le32(clock_state.high_water, out + 20);
    k_mutex_unlock(w25q64_get_mutex());
}
