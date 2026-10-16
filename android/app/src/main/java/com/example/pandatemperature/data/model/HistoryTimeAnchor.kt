package com.example.pandatemperature.data.model

import androidx.room.Entity

/** A persisted first-contact mapping; relative IDs are scoped to a device. */
@Entity(tableName = "history_time_anchors", primaryKeys = ["deviceId", "relativeStart"])
data class HistoryTimeAnchor(
    val deviceId: String,
    val relativeStart: Long,
    val relativeEnd: Long,
    val unixOffset: Long
)
