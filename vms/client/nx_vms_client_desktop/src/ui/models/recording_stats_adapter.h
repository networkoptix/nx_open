#pragma once

#include <api/model/recording_stats_reply.h>
#include <api/model/storage_status_reply.h>
#include <nx/client/core/resource/media_resource_helper.h>

class QnResourcePool;

/* Contains ready-to-use data for QnRecordingStatsModel */
struct QnCameraStatsData
{
    QnRecordingStatsReply cameras;
    QnCamRecordingStatsData foreignCameras; //< Pseudo-camera for hidden cameras.
    QnCamRecordingStatsData totals; //< Pseudo-camera stats for totals.

    QnCamRecordingStatsData chartScale; //< Limits for charting only.

    bool showForeignCameras = false;

    const QnCamRecordingStatsData& getStatsForRow(int row) const;

    int maxNameLength = 40; //< May be replaced with foreign text length if bigger.
};

namespace QnRecordingStats {

qint64 calculateAvailableSpace(const QnRecordingStatsReply& stats,
    const QnStorageSpaceDataList& storages);

QnCameraStatsData transformStatsToModelData(const QnRecordingStatsReply& stats,
    QnMediaServerResourcePtr server, QnResourcePool* resourcePool);

QnCameraStatsData forecastFromStatsToModel(const QnRecordingStatsReply& stats, qint64 totalSpace,
    QnMediaServerResourcePtr server, QnResourcePool* resourcePool);

} // namespace QnRecordingStats