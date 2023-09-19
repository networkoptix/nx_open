// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "recording_stats_adapter.h"

#include <QtCore/QCoreApplication>

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/core/storage_forecast/storage_forecaster.h>
#include <nx/utils/unicode_chars.h>

namespace {
class Translate
{
    Q_DECLARE_TR_FUNCTIONS(QnRecordingStats)
};

// Only own cameras are included in storage analytics. Others are summed to foreign.
bool isOwnCamera(const QnSecurityCamResourcePtr& resource, const QnMediaServerResourcePtr& server)
{
    return resource && resource->getParentId() == server->getId();
}

// Only active cameras are included in forecast.
bool isActive(const QnSecurityCamResourcePtr& resource, const QnCamRecordingStatsData& stats)
{
    return resource && resource->isScheduleEnabled() && resource->isOnline()
        && stats.archiveDurationSecs > 0 && stats.recordedBytes > 0;
}
} // namespace

qint64 QnRecordingStats::calculateAvailableSpace(
    const QnRecordingStatsReply& stats,
    const nx::vms::api::StorageSpaceDataListV1& storages)
{
    qint64 availableSpace = 0;

    // Add up all free non-reserved space on all storages.
    for (const auto& storage: storages)
        if (storage.isUsedForWriting && storage.isWritable && !storage.isBackup)
            availableSpace += qMax(0ll, storage.freeSpace - storage.reservedSpace);

    // Iterate over camera recordings and add those which are on re-writable storage.
    for (const auto& cameraStats: stats)
    {
        for (auto itr = cameraStats.recordedBytesPerStorage.begin();
                itr != cameraStats.recordedBytesPerStorage.end(); ++itr)
        {
            for (const auto& storageSpace: storages)
            {
                if (storageSpace.storageId == itr.key()
                    && storageSpace.isUsedForWriting
                    && storageSpace.isWritable
                    && !storageSpace.isBackup)
                {
                    availableSpace += itr.value();
                }
            }
        }
    }
    return availableSpace;
}

QnCameraStatsData QnRecordingStats::transformStatsToModelData(const QnRecordingStatsReply& stats,
    QnMediaServerResourcePtr server, QnResourcePool* resourcePool)
{
    QnCameraStatsData data = QnCameraStatsData();

    // Filter "hidden" cameras.
    for (const auto& camera: stats)
    {
        const auto& cameraResource = resourcePool->getResourceByPhysicalId<QnVirtualCameraResource>(camera.physicalId);
        if (isOwnCamera(cameraResource, server))
        {
            data.cameras << camera;
        }
        else
        {
            // Hide all cameras which belong to another server.
            data.foreignCameras.recordedBytes += camera.recordedBytes;
            data.foreignCameras.averageBitrate += camera.averageBitrate;
        }
    }
    const auto foreignText = Translate::tr("Cameras from other servers and removed cameras");
    data.foreignCameras.physicalId = foreignText; //< Use Id for camera name.
    data.showForeignCameras = data.foreignCameras.recordedBytes > 0; //< No video => no camera :-).

    // Set maxNameLength no less than foreignText.
    data.maxNameLength = qMax(data.maxNameLength, foreignText.length());

    // Create totals and chart scale data.
    for(const QnCamRecordingStatsData& value: stats)
    {
        data.totals += value;
        data.totals.averageBitrate += value.averageBitrate;

        data.chartScale.recordedBytes = qMax(data.chartScale.recordedBytes, value.recordedBytes);
        data.chartScale.recordedSecs = qMax(data.chartScale.recordedSecs, value.recordedSecs);
        data.chartScale.averageBitrate = qMax(data.chartScale.averageBitrate, value.averageBitrate);
        data.chartScale.archiveDurationSecs =
            qMax(data.chartScale.archiveDurationSecs, value.archiveDurationSecs);
    }

    // Set "totals" text to use in names column.
    QnVirtualCameraResourceList cameras;
    for (const QnCamRecordingStatsData& cameraData: data.cameras)
        if (const auto& camera = resourcePool->getResourceByPhysicalId<QnVirtualCameraResource>(cameraData.physicalId))
            cameras << camera;

    data.totals.physicalId = QnDeviceDependentStrings::getNameFromSet(
        resourcePool,
        QnCameraDeviceStringSet(
            Translate::tr("Total %1 %n devices", "%1 is long dash, do not replace",
                cameras.size()).arg(nx::UnicodeChars::kEnDash),
            Translate::tr("Total %1 %n cameras", "%1 is long dash, do not replace",
                cameras.size()).arg(nx::UnicodeChars::kEnDash),
            Translate::tr("Total %1 %n I/O modules", "%1 is long dash, do not replace",
                cameras.size()).arg(nx::UnicodeChars::kEnDash)),
        cameras);  //< Use Id for pseudo-camera name.

    return data;
}

QnCameraStatsData QnRecordingStats::forecastFromStatsToModel(const QnRecordingStatsReply& stats,
    qint64 totalSpace, //< Storage space is free storage space plus extra added storage.
    QnMediaServerResourcePtr server,
    QnResourcePool* resourcePool)
{
    using namespace nx::core::storage_forecast;
    using namespace std::chrono;

    QnRecordingStatsReply filteredStats;
    CameraRecordingSettingsSet cameras;

    // Filter "hidden" cameras.
    for (const auto& cameraStats: stats)
    {
        const auto& cameraResource =
            resourcePool->getResourceByPhysicalId<QnSecurityCamResource>(cameraStats.physicalId);

        if (isActive(cameraResource, cameraStats) && isOwnCamera(cameraResource, server))
        {
            CameraRecordingSettings camera;
            camera.physicalId = cameraStats.physicalId;
            camera.minPeriod = std::max(0s, cameraResource->minPeriod());
            camera.maxPeriod = std::max(0s, cameraResource->maxPeriod());
            camera.averageDensity = std::max(0ll, cameraStats.averageDensity);

            cameras.push_back(std::move(camera)); //< Add camera for forecast.
            filteredStats << cameraStats; //< Add camera to model.
        }
    }

    if (cameras.empty()) //< No recording cameras at all. Do not forecast anything.
        return QnCameraStatsData();

    doForecast(cameras, totalSpace, filteredStats); //< Call forecaster from core::storage_forecast.

    return transformStatsToModelData(filteredStats, server, resourcePool);  //< Return model adapted as is.
}
