#ifndef OFFLINE_CLOCK_H
#define OFFLINE_CLOCK_H
#include <stdint.h>

/* Values below 2000-01-01 are opaque relative IDs, never Unix dates. */
#define OFFLINE_TIME_LIMIT 946684800U
#define OFFLINE_CLOCK_WIRE_SIZE 24U
int offline_clock_boot(void);
uint32_t offline_clock_now(uint32_t uptime_s);
int offline_clock_anchor(uint32_t unix_s, uint32_t uptime_s);
void offline_clock_metadata(uint8_t out[OFFLINE_CLOCK_WIRE_SIZE]);
#endif
