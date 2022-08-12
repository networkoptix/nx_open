// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "storage_forecaster.h"

#include <nx/utils/log/assert.h>

#include <QtCore/QMap>

static constexpr qint64 kSecsInDay = 24 * 60 * 60;

void nx::core::storage_forecast::doForecast(const CameraRecordingSettingsSet& cameras, qint64 totalSpace,
    QnRecordingStatsReply& forecast)
{
    // First create scale for mode when some cameras can not write for corresponding minDays.
    // Get list of points where bitrate changes.
    QMap<qint64, qint64> bitrateScale;
    for (const auto& camera: cameras)
    {
        const auto cameraMinDays = std::chrono::duration_cast<std::chrono::days>(camera.minPeriod);
        bitrateScale[0] += camera.averageDensity;
        bitrateScale[cameraMinDays.count()] -= camera.averageDensity;
    }
    const qint64 maxMinDays = bitrateScale.keys().back();

    // Now append scale for mode when all cameras can write for at least their minDays.
    for (const auto& camera: cameras)
    {
        bitrateScale[maxMinDays] += camera.averageDensity;
        if (camera.maxPeriod.count() > 0) //< We consider camera.maxDays == 0s => camera should record forever.
        {
            const auto cameraMinDays =
                std::chrono::duration_cast<std::chrono::days>(camera.minPeriod);

            const auto cameraMaxDays =
                std::chrono::duration_cast<std::chrono::days>(camera.maxPeriod);

            NX_ASSERT(camera.maxPeriod >= camera.minPeriod);
            bitrateScale[cameraMaxDays.count() - cameraMinDays.count() + maxMinDays]
                -= camera.averageDensity;
        }
    }

    // Now get byte-to-day scale based on bitrate changes.
    QMap<qint64, qint64> byteScale;
    qint64 space = 0;
    qint64 currentBitrate = 0;
    qint64 lastDay = 0;
    for (const auto day: bitrateScale.keys())
    {
        space += currentBitrate * (day - lastDay) * kSecsInDay;
        byteScale[space] = day;
        currentBitrate += bitrateScale[day];
        lastDay = day;
    }

    const qint64 eternalBitrate = currentBitrate; //< Bitrate we have for eternally recording cameras.

    // Now we built the scale and can easily make forecast based on the totalSpace.
    const auto high = byteScale.upperBound(totalSpace); //< Since byteScale[0] == 0, it is not first element.
    qreal days = 0.0;
    if (high != byteScale.end()) //< We still need some space to record everything.
    {
        const auto low = high - 1;
        // Build a proportion in the space-time continuum.
        days = low.value() +
            (qreal) (high.value() - low.value()) * (totalSpace - low.key()) / (high.key() - low.key());
    }
    else
    {
        if (eternalBitrate > 0)
            days = byteScale.last() + (qreal) (totalSpace - byteScale.lastKey()) / (eternalBitrate * kSecsInDay);
        else
            days = byteScale.last();
    }

    // Now just set recording time for every camera.
    for (auto& cameraStat: forecast)
    {
        auto camera = std::find_if(cameras.begin(), cameras.end(),
            [cameraStat](const CameraRecordingSettings& cam) { return cam.physicalId == cameraStat.physicalId; });
        if (camera == cameras.end())
        {
            NX_ASSERT(false, "Strange - this cameraStat should be filtered in forecastFromStatsToModel()");
            cameraStat.archiveDurationSecs = 0; //< This camera is not in forecast (inactive etc.).
        }
        else
        {
            using DaysF = std::chrono::duration<qreal, std::chrono::days::period>;
            const auto cameraMinPeriodDaysF = std::chrono::duration_cast<DaysF>(camera->minPeriod);

            if (camera->averageDensity == 0) //< We have no data to make forecast on this camera.
            {
                cameraStat.archiveDurationSecs = 0;
            }
            else if (days < maxMinDays)
            {
                cameraStat.archiveDurationSecs =
                    qRound64(kSecsInDay * qMin(cameraMinPeriodDaysF.count(), days));
            }
            else
            {
                cameraStat.archiveDurationSecs =
                    qRound64(kSecsInDay * (cameraMinPeriodDaysF.count() + (days - maxMinDays)));
            }

            if (camera->maxPeriod.count() > 0)
            {
                cameraStat.archiveDurationSecs = std::min(
                    cameraStat.archiveDurationSecs,
                    static_cast<qint64>(camera->maxPeriod.count()));
            }
        }

        cameraStat.recordedBytes = cameraStat.archiveDurationSecs * camera->averageDensity;
    }
}
