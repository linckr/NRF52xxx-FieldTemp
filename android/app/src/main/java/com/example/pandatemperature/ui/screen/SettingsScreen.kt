package com.example.pandatemperature.ui.screen

import android.Manifest
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.PowerManager
import android.provider.Settings
import android.widget.Toast
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.KeyboardArrowRight
import androidx.compose.material.icons.filled.BatterySaver
import androidx.compose.material.icons.filled.DeviceHub
import androidx.compose.material.icons.filled.Notifications
import androidx.compose.material.icons.filled.SystemUpdate
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.example.pandatemperature.ui.viewmodel.MainViewModel

/**
 * 设置页面子页面枚举
 */
private enum class SettingsSubScreen {
    Main,            // 主设置列表
    DeviceManagement, // 设备管理
    FirmwareUpdate    // 固件升级（BLE OTA）
}

/**
 * 设置页面
 */
@Composable
fun SettingsScreen(
    viewModel: MainViewModel,
    modifier: Modifier = Modifier
) {
    // 子页面导航状态
    var currentSubScreen by remember { mutableStateOf(SettingsSubScreen.Main) }
    
    // 收集已保存设备列表
    val savedDevices by viewModel.savedDevices.collectAsStateWithLifecycle(initialValue = emptyList())
    
    val weatherAlertEnabled by viewModel.weatherAlertEnabled.collectAsStateWithLifecycle()
    val context = LocalContext.current
    // Android 13+：打开“天气预警”前必须拿到 POST_NOTIFICATIONS，否则通知会被系统静默丢弃。
    val notificationPermissionLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.RequestPermission()
    ) { granted ->
        if (granted) {
            viewModel.setWeatherAlertEnabled(true)
        } else {
            viewModel.setWeatherAlertEnabled(false)
            Toast.makeText(context, "未授予通知权限，天气预警无法提醒；请在系统设置中开启通知", Toast.LENGTH_LONG).show()
            try {
                context.startActivity(
                    Intent(Settings.ACTION_APP_NOTIFICATION_SETTINGS)
                        .putExtra(Settings.EXTRA_APP_PACKAGE, context.packageName)
                        .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                )
            } catch (_: Exception) {
            }
        }
    }
    val onWeatherToggle: (Boolean) -> Unit = { enabled ->
        if (enabled && Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU &&
            ContextCompat.checkSelfPermission(context, Manifest.permission.POST_NOTIFICATIONS) !=
            PackageManager.PERMISSION_GRANTED
        ) {
            notificationPermissionLauncher.launch(Manifest.permission.POST_NOTIFICATIONS)
        } else {
            viewModel.setWeatherAlertEnabled(enabled)
        }
    }
    when (currentSubScreen) {
        SettingsSubScreen.Main -> {
            SettingsMainScreen(
                onNavigateToDeviceManagement = { currentSubScreen = SettingsSubScreen.DeviceManagement },
                onNavigateToFirmwareUpdate = { currentSubScreen = SettingsSubScreen.FirmwareUpdate },
                weatherAlertEnabled = weatherAlertEnabled,
                onWeatherAlertEnabledChange = onWeatherToggle,
                modifier = modifier
            )
        }
        SettingsSubScreen.DeviceManagement -> {
            DeviceManagementScreen(
                devices = savedDevices,
                onBack = { currentSubScreen = SettingsSubScreen.Main },
                onDeleteDevice = { device -> viewModel.removeDevice(device) },
                modifier = modifier
            )
        }
        SettingsSubScreen.FirmwareUpdate -> {
            FirmwareUpdateScreen(
                onBack = { currentSubScreen = SettingsSubScreen.Main },
                modifier = modifier
            )
        }
    }
}

/**
 * 设置主页面 - 显示设置列表菜单
 */
@Composable
private fun SettingsMainScreen(
    onNavigateToDeviceManagement: () -> Unit,
    onNavigateToFirmwareUpdate: () -> Unit,
    weatherAlertEnabled: Boolean = true,
    onWeatherAlertEnabledChange: (Boolean) -> Unit = {},
    modifier: Modifier = Modifier
) {
    // 获取系统栏高度（全面屏安全区域）
    val systemBars = WindowInsets.systemBars
    val topPadding = systemBars.asPaddingValues().calculateTopPadding()
    val context = LocalContext.current
    val powerManager = context.getSystemService(Context.POWER_SERVICE) as PowerManager
    val ignoringBatteryOptimizations = powerManager.isIgnoringBatteryOptimizations(context.packageName)
    
    Column(
        modifier = modifier
            .fillMaxSize()
            .padding(horizontal = 16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        // 顶部间距（包含系统栏高度）
        Spacer(modifier = Modifier.height(topPadding + 8.dp))
        
        Text(
            text = "设置",
            style = MaterialTheme.typography.headlineMedium
        )
        
        // 设置列表
        Card(
            modifier = Modifier.fillMaxWidth(),
            colors = CardDefaults.cardColors(
                containerColor = MaterialTheme.colorScheme.surface
            )
        ) {
            // 天气预警
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(16.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Icon(
                    imageVector = Icons.Default.Notifications,
                    contentDescription = null,
                    modifier = Modifier.size(24.dp),
                    tint = MaterialTheme.colorScheme.primary
                )
                Spacer(modifier = Modifier.width(16.dp))
                Column(modifier = Modifier.weight(1f)) {
                    Text(text = "天气预警", style = MaterialTheme.typography.bodyLarge)
                    Text(
                        text = "气压骤降时暴风雨提醒（需连接带气压的设备）",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.outline
                    )
                }
                Switch(
                    checked = weatherAlertEnabled,
                    onCheckedChange = onWeatherAlertEnabledChange
                )
            }
            HorizontalDivider()
            // 后台稳定运行：忽略电池优化，降低长时间连接被系统限制的概率
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .clickable(enabled = !ignoringBatteryOptimizations) {
                        try {
                            context.startActivity(
                                Intent(
                                    Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS,
                                    Uri.parse("package:" + context.packageName)
                                ).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                            )
                        } catch (e: Exception) {
                            Toast.makeText(context, "无法打开电池优化设置", Toast.LENGTH_SHORT).show()
                        }
                    }
                    .padding(16.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Icon(
                    imageVector = Icons.Default.BatterySaver,
                    contentDescription = null,
                    modifier = Modifier.size(24.dp),
                    tint = MaterialTheme.colorScheme.primary
                )
                Spacer(modifier = Modifier.width(16.dp))
                Column(modifier = Modifier.weight(1f)) {
                    Text(text = "后台稳定运行", style = MaterialTheme.typography.bodyLarge)
                    Text(
                        text = if (ignoringBatteryOptimizations) "已允许忽略电池优化"
                        else "点击允许忽略电池优化，降低长连接被系统清理的概率",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.outline
                    )
                }
            }
            HorizontalDivider()
            // 设备管理
            SettingsMenuItem(
                icon = Icons.Default.DeviceHub,
                title = "设备管理",
                subtitle = "管理已保存的设备",
                onClick = onNavigateToDeviceManagement
            )
            HorizontalDivider()
            // 固件升级（BLE OTA）
            SettingsMenuItem(
                icon = Icons.Default.SystemUpdate,
                title = "固件升级",
                subtitle = "通过蓝牙升级设备固件（zephyr.signed.bin）",
                onClick = onNavigateToFirmwareUpdate
            )
        }
    }
}

/**
 * 设置菜单项
 */
@Composable
private fun SettingsMenuItem(
    icon: ImageVector,
    title: String,
    subtitle: String? = null,
    onClick: () -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(onClick = onClick)
            .padding(16.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Icon(
            imageVector = icon,
            contentDescription = null,
            modifier = Modifier.size(24.dp),
            tint = MaterialTheme.colorScheme.primary
        )
        
        Spacer(modifier = Modifier.width(16.dp))
        
        Column(modifier = Modifier.weight(1f)) {
            Text(
                text = title,
                style = MaterialTheme.typography.bodyLarge
            )
            if (subtitle != null) {
                Text(
                    text = subtitle,
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.outline
                )
            }
        }
        
        Icon(
            imageVector = Icons.AutoMirrored.Filled.KeyboardArrowRight,
            contentDescription = null,
            tint = MaterialTheme.colorScheme.outline
        )
    }
}
