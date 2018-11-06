#pragma once

#include <QtCore/QList>

#include <core/resource/resource_fwd.h>

#include "utils/common/id.h"

struct QnScheduleTask
{
    QnScheduleTask() = default;

    QnScheduleTask(
        int dayOfWeek,
        int startTime,
        int endTime,
        Qn::RecordingType recordingType,
        Qn::StreamQuality streamQuality,
        int fps,
        int bitrateKbps)
        :
        dayOfWeek(dayOfWeek),
        startTime(startTime),
        endTime(endTime),
        recordingType(recordingType),
        streamQuality(streamQuality),
        fps(fps),
        bitrateKbps(bitrateKbps)
    {
    }

    /** Day of the week, integer in range [1..7]. */
    int dayOfWeek = 1;

    /** Start time offset, in seconds. */
    int startTime = 0;

    /** End time offset, in seconds. */
    int endTime = 0;

    Qn::RecordingType recordingType = Qn::RecordingType::never;

    Qn::StreamQuality streamQuality = Qn::StreamQuality::highest;
    int fps = 10;
    int bitrateKbps = 0;

    bool operator==(const QnScheduleTask& rhs)
    {
        return std::tie(
                dayOfWeek,
                startTime,
                endTime,
                recordingType,
                streamQuality,
                fps,
                bitrateKbps)
            == std::tie(
                rhs.dayOfWeek,
                rhs.startTime,
                rhs.endTime,
                rhs.recordingType,
                rhs.streamQuality,
                rhs.fps,
                rhs.bitrateKbps);
    }

    bool isEmpty() const { return startTime == 0 && endTime == 0; }
};

Q_DECLARE_TYPEINFO(QnScheduleTask, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(QnScheduleTask)
Q_DECLARE_METATYPE(QnScheduleTaskList)
