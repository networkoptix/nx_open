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
        bitrateScale[0] += camera.averageDensity;
        bitrateScale[camera.minDays] -= camera.averageDensity;
    }
    const qint64 maxMinDays = bitrateScale.keys().back();

    // Now append scale for mode when all cameras can write for at least their minDays.
    for (const auto& camera: cameras)
    {
        bitrateScale[maxMinDays] += camera.averageDensity;
        if(camera.maxDays > 0) //< We consider camera.maxDays == 0 => camera should record forever.
        {
            NX_ASSERT(camera.maxDays >= camera.minDays);
            bitrateScale[camera.maxDays - camera.minDays + maxMinDays] -= camera.averageDensity;
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

    const qint64 maxDays = bitrateScale.keys().back(); //< After this period only eternal cameras record.
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
            [cameraStat](const CameraRecordingSettings& cam) { return cam.uniqueId == cameraStat.uniqueId; });
        if (camera == cameras.end())
        {
            NX_ASSERT(false, "Strange - this cameraStat should be filtered in forecastFromStatsToModel()");
            cameraStat.archiveDurationSecs = 0; //< This camera is not in forecast (inactive etc.).
        }
        else
        {
            if (camera->averageDensity == 0) //< We have no data to make forecast on this camera.
                cameraStat.archiveDurationSecs = 0;
            else if (days < maxMinDays)
                cameraStat.archiveDurationSecs = (qint64) (kSecsInDay * qMin((qreal)camera->minDays, days));
            else
                cameraStat.archiveDurationSecs = (qint64) (kSecsInDay * (camera->minDays + (days - maxMinDays)));

            if (camera->maxDays > 0)
                cameraStat.archiveDurationSecs = qMin(cameraStat.archiveDurationSecs, camera->maxDays * kSecsInDay);
        }

        cameraStat.recordedBytes = cameraStat.archiveDurationSecs * camera->averageDensity;
    }
}
