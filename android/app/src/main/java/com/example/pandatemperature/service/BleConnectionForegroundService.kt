package com.example.pandatemperature.service

import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Intent
import android.os.Build
import android.os.IBinder
import android.util.Log
import androidx.core.app.NotificationCompat
import com.example.pandatemperature.MainActivity
import com.example.pandatemperature.R

/**
 * 蓝牙连接前台服务：连接期间保持常驻通知，降低被系统杀进程概率。
 * 用户从最近任务划掉应用仍会断连。
 */
class BleConnectionForegroundService : Service() {

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onCreate() {
        super.onCreate()
        createNotificationChannel()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        val deviceName = intent?.getStringExtra(EXTRA_DEVICE_NAME)
            ?: getString(R.string.ble_connection_notification_device_unknown)
        val notification = buildNotification(deviceName)
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
                startForeground(NOTIFICATION_ID, notification, android.content.pm.ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE)
            } else {
                startForeground(NOTIFICATION_ID, notification)
            }
        } catch (e: Exception) {
            // Android 12+ 后台启动限制 / 缺少前台服务类型权限时，避免直接崩溃。
            Log.e(TAG, "startForeground failed; connection continues without foreground service", e)
            stopSelf()
            return START_NOT_STICKY
        }
        return START_NOT_STICKY
    }

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                CHANNEL_ID,
                getString(R.string.ble_connection_channel_name),
                NotificationManager.IMPORTANCE_LOW
            ).apply {
                description = getString(R.string.ble_connection_channel_description)
                setShowBadge(false)
            }
            (getSystemService(NOTIFICATION_SERVICE) as NotificationManager).createNotificationChannel(channel)
        }
    }

    private fun buildNotification(deviceName: String): android.app.Notification {
        val openIntent = Intent(this, MainActivity::class.java).apply {
            flags = Intent.FLAG_ACTIVITY_SINGLE_TOP or Intent.FLAG_ACTIVITY_NEW_TASK
        }
        val pendingIntent = PendingIntent.getActivity(
            this,
            0,
            openIntent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle(getString(R.string.ble_connection_notification_title))
            .setContentText(getString(R.string.ble_connection_notification_content, deviceName))
            .setSmallIcon(android.R.drawable.ic_menu_compass)
            .setContentIntent(pendingIntent)
            .setOngoing(true)
            .setCategory(NotificationCompat.CATEGORY_SERVICE)
            .build()
    }

    companion object {
        private const val TAG = "BleForegroundService"
        private const val CHANNEL_ID = "ble_connection"
        private const val NOTIFICATION_ID = 1

        const val EXTRA_DEVICE_NAME = "device_name"

        fun start(context: android.content.Context, deviceName: String?) {
            val intent = Intent(context, BleConnectionForegroundService::class.java).apply {
                putExtra(EXTRA_DEVICE_NAME, deviceName)
            }
            try {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    context.startForegroundService(intent)
                } else {
                    context.startService(intent)
                }
            } catch (e: Exception) {
                // Android 12+ 在后台启动前台服务会抛 ForegroundServiceStartNotAllowedException。
                // 连接本身仍由进程内 BLE 维持，降级为无前台服务运行，避免崩溃。
                Log.e(TAG, "startForegroundService rejected; continuing without foreground service", e)
            }
        }

        fun stop(context: android.content.Context) {
            context.stopService(Intent(context, BleConnectionForegroundService::class.java))
        }
    }
}
