#pragma once

#include <QtCore/QDate>
#include <QtCore/QLocale>

namespace nx::client::core {

class CalendarUtils
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

} // namespace nx::client::core
