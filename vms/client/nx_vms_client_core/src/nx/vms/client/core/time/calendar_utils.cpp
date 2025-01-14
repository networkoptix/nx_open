// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "calendar_utils.h"

using namespace std::chrono;

namespace nx::vms::client::core::calendar_utils {

QDate firstWeekStartDate(const QLocale& locale, int year, int month)
{
    auto date = QDate(year, month, 1);
    const auto dayOfWeek = date.dayOfWeek();
    const auto firstDayOfWeek = locale.firstDayOfWeek();

    if (dayOfWeek == firstDayOfWeek)
        return date;

    int offset{};
    if (firstDayOfWeek < dayOfWeek)
        offset = firstDayOfWeek - dayOfWeek;
    else
        offset = firstDayOfWeek - dayOfWeek - 7;

    return date.addDays(offset);
}

NX_VMS_CLIENT_CORE_API QBitArray buildArchivePresence(
    const QnTimePeriodList& timePeriods,
    const milliseconds& startTimestamp,
    int count,
    const std::function<milliseconds(milliseconds)>& nextTimestamp)
{
    QBitArray result(count);

    milliseconds startTime = startTimestamp;

    for (int i = 0; i < count; /*no increment*/)
    {
        const auto it = timePeriods.findNearestPeriod(startTime.count(), true);
        if (it == timePeriods.cend())
            break;

        const auto chunkStartTime = it->startTimeMs;
        const auto chunkEndTime = it->isInfinite()
            ? QDateTime::currentMSecsSinceEpoch()
            : it->endTimeMs();

        milliseconds endTime = nextTimestamp(startTime);
        while (i < count && endTime.count() <= chunkStartTime)
        {
            ++i;
            startTime = endTime;
            endTime = nextTimestamp(startTime);
        }

        while (i < count && startTime.count() <= chunkEndTime)
        {
            result[i] = true;
            ++i;
            startTime = nextTimestamp(startTime);
        }

        if (it->isInfinite())
            break;
    }

    return result;
}

} // namespace nx::vms::client::core::calendar_utils
