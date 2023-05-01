// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "calendar_utils.h"

namespace nx::vms::client::core::calendar_utils {

QDate firstWeekStartDate(const QLocale& locale, int year, int month)
{
    auto date = QDate(year, month, 1);
    const int dayOfWeek = date.dayOfWeek();
    if (locale.firstDayOfWeek() == Qt::Monday)
        date = date.addDays(1 - dayOfWeek);
    else if (dayOfWeek < Qt::Sunday)
        date = date.addDays(-dayOfWeek);
    return date;
}

} // namespace nx::vms::client::core::calendar_utils
