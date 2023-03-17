// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "schedule.h"

#include <chrono>

#include <QtCore/QDateTime>

#include <utils/media/bitStream.h>

namespace nx::vms::common {

using namespace std::chrono;

namespace {

constexpr auto kBufferSize = kHoursInDay * kDaysInWeek / 8;

int hToS(int h)
{
    return duration_cast<seconds>(hours(h)).count();
}

int sToH(int s)
{
    return duration_cast<hours>(seconds(s)).count();
}

} // namespace

const std::array<Qt::DayOfWeek, kDaysInWeek>& daysOfWeek()
{
    static constexpr std::array<Qt::DayOfWeek, kDaysInWeek> result = {
        Qt::Monday, Qt::Tuesday, Qt::Wednesday, Qt::Thursday, Qt::Friday, Qt::Saturday, Qt::Sunday
    };

    return result;
}

nx::vms::api::CameraScheduleTaskDataList defaultSchedule(int maxFps)
{
    nx::vms::api::CameraScheduleTaskDataList schedule;
    for (qint8 dayOfWeek = 1; dayOfWeek <= 7; ++dayOfWeek)
    {
        schedule.push_back(nx::vms::api::CameraScheduleTaskData{
            /*start time*/ 0,
            seconds(24h).count(),
            dayOfWeek,
            nx::vms::api::RecordingType::metadataOnly,
            nx::vms::api::StreamQuality::high,
            (qint16) maxFps,
            /*automatic bitrate*/ 0,
            {nx::vms::api::RecordingMetadataType::motion}});
    }

    return schedule;
}

nx::vms::api::ScheduleTaskDataList scheduleFromByteArray(const QByteArray& schedule)
{
    if (schedule.isEmpty())
        return {};

    if (!NX_ASSERT(schedule.size() >= kBufferSize))
        return {};

    if (std::all_of(schedule.begin(), schedule.end(), [](char c) { return c == -1; }))
        return {};

    nx::vms::api::ScheduleTaskDataList tasks;
    BitStreamReader reader((uint8_t*)schedule.data(), schedule.size());

    for (const auto dayOfWeek: daysOfWeek())
    {
        nx::vms::api::ScheduleTaskData task;

        for (int hour = 0; hour < kHoursInDay; ++hour)
        {
            const bool on = reader.getBit();

            if (!on && !task.isValid())
                continue;

            if (on && !task.isValid())
            {
                task.dayOfWeek = dayOfWeek;
                task.startTime = hToS(hour);
                task.endTime = hToS(hour + 1);
            }
            else if (on && task.isValid())
            {
                task.endTime = hToS(hour + 1);
            }
            else
            {
                tasks.push_back(task);
                task = {};
            }
        }

        if (task.isValid())
            tasks.push_back(task);
    }

    return tasks;
}

QByteArray scheduleToByteArray(const nx::vms::api::ScheduleTaskDataList& schedule)
{
    if (schedule.empty())
        return {};

    QByteArray buffer(kBufferSize, 0);

    for (const auto& task: schedule)
    {
        const auto dayOfWeek = static_cast<Qt::DayOfWeek>(task.dayOfWeek);

        for (int hour = sToH(task.startTime); hour < sToH(task.endTime); ++hour)
        {
            int currentWeekHour = (dayOfWeek - 1) * 24 + hour;
            int byteOffset = currentWeekHour / 8;

            int bitNum = 7 - (currentWeekHour % 8);
            quint8 mask = 1 << bitNum;

            buffer[byteOffset] = buffer[byteOffset] | mask;
        }
    }

    return buffer;
}

bool timeInSchedule(QDateTime datetime, const QByteArray& schedule)
{
    if (schedule.isEmpty())
        return true;

    int currentWeekHour = (datetime.date().dayOfWeek() - 1) * 24 + datetime.time().hour();

    int byteOffset = currentWeekHour / 8;
    if (byteOffset >= schedule.size())
        return false;

    int bitNum = 7 - (currentWeekHour % 8);
    quint8 mask = 1 << bitNum;

    return (schedule.at(byteOffset) & mask);
}

} // namespace nx::vms::common
