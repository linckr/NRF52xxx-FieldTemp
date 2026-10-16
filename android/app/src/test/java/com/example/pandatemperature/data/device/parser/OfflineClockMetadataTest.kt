package com.example.pandatemperature.data.device.parser

import com.example.pandatemperature.data.device.profile.DeviceProfileFactory
import org.junit.Assert.*
import org.junit.Test
import java.nio.ByteBuffer
import java.nio.ByteOrder

class OfflineClockMetadataTest {
    @Test fun startupZerosAndInvalidSentinelsDoNotBecomePhoneSamples() {
        val parser = RealtimeDataParserV2()
        assertNull(parser.parse(ByteArray(8)))
        val invalid = parser.parse(byteArrayOf(0, 0x80.toByte(), -1, -1, -1, -1, -1, -1))!!
        assertNull(invalid.temperature)
        assertNull(invalid.humidity)
        assertNull(invalid.pressure)
        val realZero = ByteBuffer.allocate(6).order(ByteOrder.LITTLE_ENDIAN)
            .putShort(0).putShort(0).putShort(10132).array()
        assertEquals(0f, parser.parse(realZero)!!.temperature!!, 0f)
        assertEquals(0f, parser.parse(realZero)!!.humidity!!, 0f)
    }
    private fun metadata(base: Int, utc: Int, uptime: Int, high: Int = base + 3600) =
        ByteBuffer.allocate(24).order(ByteOrder.LITTLE_ENDIAN)
            .putInt(0x31434c4f).putInt(base + 3600).putInt(base).putInt(utc).putInt(uptime).putInt(high).array()

    @Test fun firstConnectionReconstructsOnlyTheSameReservedSession() {
        val anchor = OfflineClockMetadata.parseAnchor("a", metadata(7201, 1_792_100_000, 1800))!!
        assertEquals(1_792_098_800L, 7801L + anchor.unixOffset)
        assertEquals(7201L, anchor.relativeStart)
        assertEquals(9001L, anchor.relativeEnd)
        assertTrue(3601L < anchor.relativeStart) // Previous uncalibrated boot is outside the range.
    }

    @Test fun invalidOrUncalibratedMetadataCannotCreateAnAnchor() {
        assertNull(OfflineClockMetadata.parseAnchor("a", metadata(0, 0, 0)))
        assertNull(OfflineClockMetadata.parseAnchor("a", metadata(1, 1_792_100_000, 5000, 3600)))
        assertNull(OfflineClockMetadata.parseAnchor("a", metadata(1, 100, 10)))
        assertNull(OfflineClockMetadata.parseAnchor("a", ByteArray(24)))
        assertNull(OfflineClockMetadata.parseAnchor("a", ByteArray(23)))
    }

    @Test fun relativeFormatRequiresExplicitVersionAndOptIn() {
        val modern = DeviceProfileFactory.createThermometerProfile(12, capabilityFlags = 0xff)
        val legacy = DeviceProfileFactory.createThermometerProfile(11, capabilityFlags = 0xff)
        assertEquals(5.toByte(), modern.buildHistoryRequest(0).last())
        assertEquals(3.toByte(), legacy.buildHistoryRequest(0).last())
        val packet = ByteBuffer.allocate(14).order(ByteOrder.LITTLE_ENDIAN)
            .putInt(7801).putShort(2500).putShort(5000).putInt(101325).putShort(2800).array()
        val row = modern.getHistoryParser("a").parse(packet)!!.single()
        assertTrue(row.timeUncertain)
        assertEquals(7801L, row.rawTimestamp)
        assertNull(legacy.getHistoryParser("a").parse(packet)!!.single().rawTimestamp)
    }
}
