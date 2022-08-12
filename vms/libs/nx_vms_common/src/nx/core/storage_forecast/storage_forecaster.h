// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QVector>

#include <api/model/recording_stats_reply.h>

namespace nx::core::storage_forecast {

struct CameraRecordingSettings
{
    // These are taken from QnCamRecordingStatsData.
    QString physicalId;
    qint64 averageDensity = 0; //< Average density (= bytes / requested period in seconds).

    // These are taken from ResourcePool
    std::chrono::seconds minPeriod{0}; //< Cached camera 'minPeriod' value.
    std::chrono::seconds maxPeriod{0}; //< Cached camera 'maxPeriod' value. maxPeriod == 0s => record forever.
};

using CameraRecordingSettingsSet = QVector<CameraRecordingSettings>;

// Creates forecast. Sets `archiveDurationSecs` field in forecast structure based
// on `cameras` and `totalSpace`.
NX_VMS_COMMON_API void doForecast(
    const CameraRecordingSettingsSet& cameras,
    qint64 totalSpace,
    QnRecordingStatsReply& forecast);

} // namespace nx::core::storage_forecast
