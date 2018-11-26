#pragma once

#include <QtCore/QVector>

#include <api/model/recording_stats_reply.h>

namespace nx::core::storage_forecast {

struct CameraData
{
    // These are taken from QnCamRecordingStatsData.
    QString uniqueId;
    qint64 averageDensity = 0; //< Average density (= bytes / requested period in seconds).

    // These are taken from ResourcePool
    int minDays = 0; //< Cached camera 'minDays' value.
    int maxDays = 0; //< Cached camera 'maxDays' value. maxDays == 0 => record forever.
};

using AllCameraData = QVector<CameraData>;

// Creates forecast. Sets `archiveDurationSecs` field in forecast structure based
// on `cameras` and `totalSpace`.
void doForecast(const AllCameraData& cameras, qint64 totalSpace, QnRecordingStatsReply& forecast);

} // namespace nx::core::storage_forecast
