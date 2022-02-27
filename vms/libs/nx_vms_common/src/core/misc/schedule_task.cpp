// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "schedule_task.h"

#include <chrono>

using namespace std::chrono;

QnScheduleTaskList defaultSchedule(int maxFps)
{
    QnScheduleTaskList schedule;
    for (qint8 dayOfWeek = 1; dayOfWeek <= 7; ++dayOfWeek)
    {
        schedule.push_back(QnScheduleTask{
            /*start time*/ 0,
            seconds(24h).count(),
            nx::vms::api::RecordingType::metadataOnly,
            dayOfWeek,
            nx::vms::api::StreamQuality::high,
            (qint16) maxFps,
            /*automatic bitrate*/ 0,
            {nx::vms::api::RecordingMetadataType::motion}});
    }
    return schedule;
}
