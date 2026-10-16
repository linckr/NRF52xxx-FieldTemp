package com.example.pandatemperature

import android.content.Intent
import android.os.Bundle
import android.os.SystemClock
import androidx.lifecycle.ViewModelProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.runner.lifecycle.ActivityLifecycleMonitorRegistry
import androidx.test.runner.lifecycle.Stage
import com.example.pandatemperature.data.bluetooth.BleConstants
import com.example.pandatemperature.data.bluetooth.BleManager
import com.example.pandatemperature.data.database.AppDatabase
import com.example.pandatemperature.data.device.parser.OfflineClockMetadata
import com.example.pandatemperature.ui.viewmodel.MainViewModel
import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.NonCancellable
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeout
import org.junit.Assert.*
import org.junit.Assume.assumeTrue
import org.junit.Test
import org.junit.runner.RunWith

/** Opt-in normal App calibration; never clears data or resets the module. */
@RunWith(AndroidJUnit4::class)
class LiveOfflineClockTest {
    @Test fun verifyCurrentBootCalibration() = runBlocking<Unit> {
        val args = InstrumentationRegistry.getArguments()
        assumeTrue("Explicit verification required", args.getString("offlineClockAction") == "verify")
        val expectedBase = requireNotNull(args.getString("expectedBootBase")).toLong()
        val minimumRecords = args.getString("minimumRelativeRecords")?.toInt() ?: 1
        require(expectedBase > 0 && minimumRecords > 0)
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        val context = instrumentation.targetContext
        context.startActivity(Intent(context, MainActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
        var activity: MainActivity? = null
        val activityDeadline = SystemClock.elapsedRealtime() + 30_000
        while (activity == null) {
            instrumentation.runOnMainSync {
                activity = ActivityLifecycleMonitorRegistry.getInstance().getActivitiesInStage(Stage.RESUMED)
                    .filterIsInstance<MainActivity>().firstOrNull()
            }
            assertTrue("Activity not resumed", SystemClock.elapsedRealtime() < activityDeadline)
            Thread.sleep(50)
        }
        lateinit var vm: MainViewModel
        instrumentation.runOnMainSync { vm = ViewModelProvider(requireNotNull(activity))[MainViewModel::class.java] }
        val connection = SavedMaintenanceConnection.create(context, vm, args.getString("targetDeviceSha256"))
        var exclusive = false
        try {
            val deadline = SystemClock.elapsedRealtime() + 180_000
            var nextReport = 0L
            while (!exclusive) {
                connection.connectIfDisconnected()
                if (vm.connectionState.value == BleManager.ConnectionState.ServicesDiscovered &&
                    vm.temperature.value != null && !vm.isFetchingHistory.value &&
                    vm.logs.value.any { it.message.contains("历史数据传输完成") }) {
                    exclusive = vm.beginExclusiveOta()
                }
                assertTrue("Connection/history did not become idle", SystemClock.elapsedRealtime() < deadline)
                if (!exclusive && SystemClock.elapsedRealtime() >= nextReport) {
                    instrumentation.sendStatus(0, Bundle().apply {
                        putString("clock_checkpoint", "await_normal_connection_and_sync")
                        putString("connection_state", vm.connectionState.value.toString())
                        putBoolean("history_fetching", vm.isFetchingHistory.value)
                    })
                    nextReport = SystemClock.elapsedRealtime() + 5_000
                }
                if (!exclusive) Thread.sleep(50)
            }
            val address = requireNotNull(vm.deviceAddress.value)
            val reply = CompletableDeferred<ByteArray?>()
            BleManager.getInstance(context).readCharacteristic(BleConstants.TIME_SYNC_CHAR) { reply.complete(it) }
            val bytes = requireNotNull(withTimeout(10_000L) { reply.await() })
            assertEquals(24, bytes.size)
            val metadata = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN)
            assertEquals(expectedBase, metadata.getInt(4).toLong() and 0xffffffffL)
            val anchor = requireNotNull(OfflineClockMetadata.parseAnchor(address, bytes))
            assertEquals("Expected current boot's persisted anchor", expectedBase, anchor.relativeStart)
            val dao = AppDatabase.getDatabase(context).temperatureRecordDao()
            assertEquals("Phone must preserve the same mapping", anchor, dao.findTimeAnchor(address, expectedBase))
            val rows = dao.getAllRecordsSync(address).filter {
                it.rawTimestamp != null && it.rawTimestamp!! in anchor.relativeStart..anchor.relativeEnd
            }
            assertTrue("No pre-calibration module history recovered", rows.size >= minimumRecords)
            rows.forEach { row ->
                assertFalse(row.isPhoneSample)
                assertNull(row.latitude)
                assertNull(row.longitude)
                assertFalse("Mapped record still uncertain", row.timeUncertain)
                assertEquals(requireNotNull(row.rawTimestamp) + anchor.unixOffset, row.timestamp)
                assertTrue("Mapped date implausibly in future", row.timestamp <= System.currentTimeMillis() / 1000 + 5)
            }
            val unknownStart = args.getString("expectedUnknownStart")?.toLong()
            val unknownEnd = args.getString("expectedUnknownEndExclusive")?.toLong()
            var unknownCount = 0
            if (unknownStart != null || unknownEnd != null) {
                require(unknownStart != null && unknownEnd != null && unknownStart < unknownEnd)
                val unknown = dao.getAllRecordsSync(address).filter {
                    it.rawTimestamp != null && it.rawTimestamp!! >= unknownStart && it.rawTimestamp!! < unknownEnd
                }
                assertTrue("Missing previous unanchored boot's history", unknown.isNotEmpty())
                unknown.forEach { row ->
                    assertTrue("Unanchored boot must remain uncertain", row.timeUncertain)
                    assertEquals(requireNotNull(row.rawTimestamp), row.timestamp)
                    assertNull("Unanchored boot must not acquire another boot's map", dao.findTimeAnchor(address, row.rawTimestamp!!))
                }
                unknownCount = unknown.size
            }
            instrumentation.sendStatus(0, Bundle().apply {
                putString("clock_checkpoint", "CURRENT_BOOT_HISTORY_CALIBRATED")
                putLong("boot_base", expectedBase)
                putInt("reconstructed_records", rows.size)
                putInt("previous_boot_unknown_records", unknownCount)
                putString("result", "PASS")
            })
        } finally {
            if (exclusive) withContext(NonCancellable) { vm.endExclusiveOta() }
        }
    }
}
