// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "timeline_zoom_level.h"

#include <QtCore/QDateTime>
#include <QtCore/QVector>

#include <nx/utils/log/assert.h>
#include <nx/vms/time/formatter.h>

namespace nx::vms::client::core {

namespace {

constexpr int kOneHourMs = 60 * 60 * 1000;

} // namespace

bool TimelineZoomLevel::testTick(qint64 tick, const QTimeZone& tz) const
{
    QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(tick, tz);
    if (isMonotonic())
    {
        const qint64 fromStartOfDay = tick - dateTime.date().startOfDay(tz).toMSecsSinceEpoch();
        return (fromStartOfDay % interval) == 0;
    }

    if (type == Hours)
    {
        const auto time = dateTime.time();
        return time.minute() == 0 && time.second() == 0 && time.msec() == 0
            && (time.hour() % (interval / kOneHourMs)) == 0;
    }

    const bool isStartOfDay = dateTime.time() == QTime(0, 0);
    if (!isStartOfDay)
        return false;

    const QDate date = dateTime.date();
    if (type == Days)
    {
        if (interval > 1 && date.day() > 25)
            return false;

        return date.day() == 1 || date.day() % interval == 0;
    }

    if (date.day() != 1)
        return false;

    if (type == Months)
        return (date.month() - 1) % interval == 0;

    if (dateTime.date().month() != 1)
        return false;

    return date.year() % interval == 0;
}

qint64 TimelineZoomLevel::nextTick(qint64 tick, const QTimeZone& tz) const
{
    switch (type)
    {
        case Hours:
        {
            if (interval == kOneHourMs)
                return tick + interval;

            auto dateTime = QDateTime::fromMSecsSinceEpoch(tick, tz);
            const int step = interval / kOneHourMs;

            int hours = dateTime.time().hour();
            hours = hours - (hours % step) + step;

            if (hours < 24)
            {
                dateTime.setTime(QTime(hours, 0));
                return dateTime.toMSecsSinceEpoch();
            }

            dateTime.setTime(QTime(0, 0));
            return dateTime.addDays(1).toMSecsSinceEpoch();
        }

        case Days:
        {
            const auto dateTime = QDateTime::fromMSecsSinceEpoch(tick, tz);
            if (interval == 1)
                return dateTime.addDays(interval).toMSecsSinceEpoch();

            const auto date = dateTime.date();

            const auto nextDateTime = dateTime.addDays(
                dateTime.date().day() == 1 ? (interval - 1) : interval);

            if (nextDateTime.date().month() != dateTime.date().month()
                || (nextDateTime.date().day() + interval) > 31)
            {
                return QDateTime(QDate(date.year(), date.month(), 1), QTime{}).addMonths(1)
                    .toMSecsSinceEpoch();
            }

            return nextDateTime.toMSecsSinceEpoch();
        }

        case Months:
            return QDateTime::fromMSecsSinceEpoch(tick, tz).addMonths(interval).toMSecsSinceEpoch();

        case Years:
            return QDateTime::fromMSecsSinceEpoch(tick, tz).addYears(interval).toMSecsSinceEpoch();

        default:
            return tick + interval;
    }
}

qint64 TimelineZoomLevel::averageTickLength() const
{
    static constexpr auto kDayMsecs = 1000ll * 60 * 60 * 24;

    switch (type)
    {
        case Days:
            return kDayMsecs * interval;
        case Months:
            return kDayMsecs * 30 * interval;
        case Years:
            return kDayMsecs * 30 * 12 * interval;
        default:
            return interval;
    }
}

qint64 TimelineZoomLevel::alignTick(qint64 tick, const QTimeZone& tz) const
{
    const QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(tick, tz);
    QDate date = dateTime.date();

    if (isMonotonic())
    {
        const qint64 fromStartOfDay = tick - date.startOfDay(tz).toMSecsSinceEpoch();
        return tick - (fromStartOfDay % interval);
    }

    switch (type)
    {
        case Hours:
        {
            const int step = interval / kOneHourMs;
            int hours = dateTime.time().hour();
            hours -= hours % step;
            return QDateTime(date, QTime(hours, 0), tz).toMSecsSinceEpoch();
        }

        case Days:
        {
            if (interval > 1)
            {
                int day = qMax(1, date.day() - date.day() % int(interval));
                if (day > 25)
                    day -= interval;
                date = QDate(date.year(), date.month(), day);
            }
            break;
        }

        case Months:
        {
            int month = date.month() - 1;
            month -= month % interval;
            date = QDate(date.year(), month + 1, 1);
            break;
        }

        case Years:
        {
            const int year = date.year() - date.year() % interval;
            date = QDate(year, 1, 1);
            break;
        }

        default:
            break;
    }

    return date.startOfDay(tz).toMSecsSinceEpoch();
}

int TimelineZoomLevel::tickCount(qint64 start, qint64 end, const QTimeZone& tz) const
{
    if (start >= end)
        return 1;

    if (type < Days)
        return qMax(static_cast<int>((end - start) / interval), 1);

    const QDate startDate = QDateTime::fromMSecsSinceEpoch(start, tz).date();
    const QDate endDate = QDateTime::fromMSecsSinceEpoch(end, tz).date();

    switch (type)
    {
        case Days:
            return startDate.daysTo(endDate) / interval;

        case Months:
            return qMax((12 - startDate.month()) + endDate.month()
                + 12 * (endDate.year() - startDate.year() - 1) - 1, 1) / interval;

        case Years:
            return qMax((endDate.year() - startDate.year()) / static_cast<int>(interval), 1);

        default:
            NX_ASSERT(false);
            return 0;
    }
}

bool TimelineZoomLevel::isMonotonic() const
{
    return type < Hours || (type == Hours && interval == kOneHourMs);
}

QString TimelineZoomLevel::value(qint64 tick, const QTimeZone& tz) const
{
    const auto dateTime = QDateTime::fromMSecsSinceEpoch(tick, tz);
    switch (type)
    {
        case Milliseconds:
            return dateTime.toString(QStringLiteral("zzz"));
        case Seconds:
            return dateTime.toString(QStringLiteral("s"));
        case Minutes:
        case Hours:
            return QString("%1:%2").arg(
                nx::vms::time::toString(dateTime, nx::vms::time::Format::h),
                nx::vms::time::toString(dateTime, nx::vms::time::Format::m));
        case Days:
            return dateTime.toString(QStringLiteral("d"));
        case Months:
            return QString();
        case Years:
            return dateTime.toString(QStringLiteral("yyyy"));
    }
    return QString();
}

QString TimelineZoomLevel::suffix(qint64 tick, const QTimeZone& tz) const
{
    QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(tick, tz);
    switch (type)
    {
        case Days:
            return QLocale().monthName(dateTime.date().month(), QLocale::ShortFormat);
        case Months:
            return QLocale().standaloneMonthName(dateTime.date().month(), QLocale::ShortFormat);
        case Milliseconds:
            return QStringLiteral("ms");
        case Seconds:
            return QStringLiteral("s");
        case Minutes:
        case Hours:
            return nx::vms::time::toString(dateTime, nx::vms::time::Format::a);
        default:
            return QString();
    }
}

QString TimelineZoomLevel::longestText() const
{
    switch (type)
    {
        case Milliseconds:
            return QStringLiteral("000 ms");
        case Seconds:
            return QStringLiteral("00 s");
        case Minutes:
        case Hours:
            return nx::vms::time::is24HoursTimeFormat()
                ? QStringLiteral("00:00")
                : QStringLiteral("00:00 MM");
        case Years:
            return QStringLiteral("0000");
        case Days:
        case Months:
            // Cannot be evaluated accurately.
            // The caller should measure text strings by itself.
            return QString();
        default:
            return QString();
    }
}

} // namespace nx::vms::client::core
