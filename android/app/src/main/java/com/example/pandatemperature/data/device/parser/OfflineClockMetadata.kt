package com.example.pandatemperature.data.device.parser

import com.example.pandatemperature.data.model.HistoryTimeAnchor
import java.nio.ByteBuffer
import java.nio.ByteOrder

/** OLC1 metadata on the existing time characteristic (firmware >= 12). */
object OfflineClockMetadata {
    const val UNIX_MIN = 946684800L
    const val UNIX_MAX = 4102444800L

    fun parseAnchor(deviceId: String, data: ByteArray): HistoryTimeAnchor? {
        if (data.size != 24) return null
        val b = ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN)
        fun u32(offset: Int) = b.getInt(offset).toLong() and 0xffffffffL
        if (u32(0) != 0x31434c4fL) return null
        val base = u32(8)
        val currentBase = u32(4)
        val unix = u32(12)
        val uptime = u32(16)
        val highWater = u32(20)
        if (currentBase == 0L || currentBase > highWater || highWater >= UNIX_MIN ||
            base == 0L || base > currentBase || (base != currentBase && base + uptime >= currentBase) ||
            base + uptime >= UNIX_MIN || base + uptime > highWater ||
            unix !in UNIX_MIN..UNIX_MAX || unix - uptime < UNIX_MIN) return null
        return HistoryTimeAnchor(deviceId, base, base + uptime, unix - uptime - base)
    }
}
