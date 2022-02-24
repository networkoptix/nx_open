// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "calendar_utils.h"

#include <limits>
#include <chrono>

#include <QtCore/QDateTime>

namespace {

static constexpr int kMaxOffset =
    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::hours(14)).count();

} // namespace

namespace nx::vms::client::core {

const int CalendarUtils::kMinYear = QDateTime::fromMSecsSinceEpoch(0, Qt::UTC).date().year();
const int CalendarUtils::kMaxYear = QDateTime::fromMSecsSinceEpoch(
    std::numeric_limits<qint64>::max(), Qt::UTC).date().year();

const int CalendarUtils::kMinMonth = 1;
const int CalendarUtils::kMaxMonth = 12;

// According to the Qt documentation time zones offset range is in [-14..14] hours range.
const int CalendarUtils::kMinDisplayOffset = -kMaxOffset;
const int CalendarUtils::kMaxDisplayOffset = kMaxOffset;

QDate CalendarUtils::firstWeekStartDate(const QLocale& locale, int year, int month)
{
    auto date = QDate(year, month, 1);
    const int dayOfWeek = date.dayOfWeek();
    if (locale.firstDayOfWeek() == Qt::Monday)
        date = date.addDays(1 - dayOfWeek);
    else if (dayOfWeek < Qt::Sunday)
        date = date.addDays(-dayOfWeek);
    return date;
}

} // namespace nx::vms::client::core
