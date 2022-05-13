// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QDate>
#include <QtCore/QLocale>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API CalendarUtils
{
public:
    static const int kMinYear;
    static const int kMaxYear;

    static const int kMinMonth;
    static const int kMaxMonth;

    static const int kMinDisplayOffset;
    static const int kMaxDisplayOffset;

    static QDate firstWeekStartDate(const QLocale& locale, int year, int month);

private:
    CalendarUtils() = default;
};

} // namespace nx::vms::client::core
