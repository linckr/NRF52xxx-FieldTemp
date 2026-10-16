package com.example.pandatemperature

import android.app.Application
import android.bluetooth.BluetoothManager
import android.content.Context
import android.content.pm.PackageManager
import android.Manifest
import android.os.SystemClock
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.ViewModelStore
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import com.example.pandatemperature.data.database.AppDatabase
import com.example.pandatemperature.ui.viewmodel.MainViewModel
import kotlinx.coroutines.runBlocking
import org.junit.Assert.*
import org.junit.Assume.assumeTrue
import org.junit.Test
import org.junit.runner.RunWith

/** Opt-in read-only regression against existing data. Does not connect, seed or clear data. */
@RunWith(AndroidJUnit4::class)
class OfflineHistoryTest {
    @Test
    fun coldViewModelLoadsSavedHistoryWithBluetoothOff() = runBlocking {
        val noConnectPermission = InstrumentationRegistry.getArguments().getString("offlineNoConnectPermissionCheck") == "true"
        assumeTrue(noConnectPermission || InstrumentationRegistry.getArguments().getString("offlineHistoryCheck") == "true")
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        val context = instrumentation.targetContext
        val adapter = (context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager).adapter
        if (noConnectPermission) {
            assertEquals(PackageManager.PERMISSION_DENIED, context.checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT))
        } else {
            assertFalse("Operator must switch Bluetooth off before this test", adapter.isEnabled)
        }
        val database = AppDatabase.getDatabase(context)
        val saved = requireNotNull(database.deviceDao().getLatestDevice())
        val dao = database.temperatureRecordDao()
        val expected = dao.getLatestRecords(saved.macAddress, 1)
        assertTrue("Requires existing local history", expected.isNotEmpty())
        val countBefore = dao.getRecordCount(saved.macAddress)
        val store = ViewModelStore()
        lateinit var vm: MainViewModel
        instrumentation.runOnMainSync {
            vm = ViewModelProvider(store, ViewModelProvider.AndroidViewModelFactory.getInstance(
                context.applicationContext as Application))[MainViewModel::class.java]
            // Reproduce the history tab entering before async device selection has finished.
            vm.loadHistoryDetail()
        }
        try {
            val deadline = SystemClock.elapsedRealtime() + 15_000
            while (vm.historyDetailRecords.value.isEmpty() && SystemClock.elapsedRealtime() < deadline) {
                Thread.sleep(50)
            }
            assertEquals(expected.first().id, vm.historyDetailRecords.value.firstOrNull()?.id)
            assertTrue(vm.historyDetailRecords.value.all { it.deviceId == saved.macAddress })
            if (!noConnectPermission) assertFalse(adapter.isEnabled)
            assertEquals(countBefore, dao.getRecordCount(saved.macAddress))
        } finally {
            instrumentation.runOnMainSync { store.clear() }
        }
    }
}
