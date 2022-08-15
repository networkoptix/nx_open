// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/core/storage_forecast/storage_forecaster.h>
#include <gtest/gtest.h>

using namespace nx::core::storage_forecast;

static constexpr qint64 kSecsInDay = 24 * 60 * 60;

TEST(storage_forecast, Basic)
{
    // Create test camera data.
    CameraRecordingSettingsSet allCameras = {
        {"Camera 1", 1000, std::chrono::days(0), std::chrono::days(1)},
        {"Camera 2", 2000, std::chrono::days(0), std::chrono::days(3)},
        {"Camera 3", 3000, std::chrono::days(1), std::chrono::days(2)},
        {"Camera 4", 4000, std::chrono::days(2), std::chrono::days(3)},
        {"Camera 5", 5000, std::chrono::days(4), std::chrono::days(7)}
    };


    QnRecordingStatsReply recordingStats;
    for (auto& camera: allCameras)
    {
        QnCamRecordingStatsData cameraStats;
        cameraStats.physicalId = camera.physicalId; //< Anything else is just not needed.
        recordingStats.push_back(std::move(cameraStats));
    }

    // Do forecast 1.
    // 2 secs for cams 3,4,5:
    doForecast(allCameras, 2 * (3000 + 4000 + 5000), recordingStats);
    QVector<qint64> forecast = {0, 0, 2, 2, 2};
    for (int i = 0; i < 5; i++)
        ASSERT_TRUE(forecast[i] == recordingStats[i].archiveDurationSecs);

    // 1 day for cam 3, 2 days for cam 4, 3 days for cam 5:
    doForecast(allCameras, kSecsInDay * (1 * 3000 + 2 * 4000 + 3 * 5000), recordingStats);
    forecast = {0, 0, 1 * kSecsInDay, 2 * kSecsInDay, 3 * kSecsInDay};
    for (int i = 0; i < 5; i++)
        ASSERT_TRUE(forecast[i] == recordingStats[i].archiveDurationSecs);

    //  1 day for cam 3, 2 days for cam 4, 3 days + 10 sec for cam 5:
    doForecast(allCameras, kSecsInDay * (1 * 3000 + 2 * 4000 + 3 * 5000) + 10 * 5000, recordingStats);
    forecast = {0, 0, 1 * kSecsInDay, 2 * kSecsInDay, 3 * kSecsInDay + 10};
    for (int i = 0; i < 5; i++)
        ASSERT_TRUE(forecast[i] == recordingStats[i].archiveDurationSecs);

    //  1 day for cam 1, 2 days for cam 2, 2 days for cam 3, 3 days for cam 4,  6 days for cam 5:
    doForecast(allCameras, kSecsInDay * (1 * 1000 + 2 * 2000 + 2 * 3000 + 3 * 4000 + 6 * 5000),
        recordingStats);
    forecast = {1 * kSecsInDay, 2 * kSecsInDay, 2 * kSecsInDay, 3 * kSecsInDay, 6 * kSecsInDay};
    for (int i = 0; i < 5; i++)
        ASSERT_TRUE(forecast[i] == recordingStats[i].archiveDurationSecs);

    //  Enough for all cams + a lot of space:
    doForecast(allCameras, kSecsInDay * (1 * 1000 + 3 * 2000 + 2 * 3000
        + 3 * 4000 + 7 * 5000 + 100000), recordingStats);
    forecast = {1 * kSecsInDay, 3 * kSecsInDay, 2 * kSecsInDay, 3 * kSecsInDay, 7 * kSecsInDay};
    for (int i = 0; i < 5; i++)
        ASSERT_TRUE(forecast[i] == recordingStats[i].archiveDurationSecs);

    // Forecast 2.
    // Now add extra cam that records forever.
    allCameras.push_back({"Camera 6", 6000, std::chrono::days(0), std::chrono::days(0)});
    QnCamRecordingStatsData cameraStats;
    cameraStats.physicalId = allCameras[5].physicalId;
    recordingStats.push_back(std::move(cameraStats));

    // 1 day for cam 3, 2 days for cam 4, 3 days for cam 5:
    doForecast(allCameras, kSecsInDay * (1 * 3000 + 2 * 4000 + 3 * 5000), recordingStats);
    forecast = {0, 0, 1 * kSecsInDay, 2 * kSecsInDay, 3 * kSecsInDay, 0 };
    for (int i = 0; i < 6; i++)
        ASSERT_TRUE(forecast[i] == recordingStats[i].archiveDurationSecs);

    //  Enough for all cams + 10 days for last cam:
    doForecast(allCameras, kSecsInDay * ( 1 * 1000 + 3 * 2000 + 2 * 3000
        + 3 * 4000 + 7 * 5000 + 10 * 6000), recordingStats);
    forecast = {1 * kSecsInDay, 3 * kSecsInDay, 2 * kSecsInDay, 3 * kSecsInDay, 7 * kSecsInDay, 10 * kSecsInDay};
    for (int i = 0; i < 6; i++)
        ASSERT_TRUE(forecast[i] == recordingStats[i].archiveDurationSecs);
}

