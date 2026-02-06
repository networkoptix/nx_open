// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "timeline_label_formatter.h"

#include <QtQml/QtQml>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/format.h>

namespace nx::vms::client::core {
namespace timeline {

using namespace std::chrono;

struct LabelFormatter::Private
{
    QLocale locale;
    bool amPm = is12HourFormat();

    bool is12HourFormat() const
    {
        return locale.timeFormat().contains("AP", Qt::CaseInsensitive);
    }
};

LabelFormatter::LabelFormatter(QObject* parent):
    QObject(parent),
    d(new Private{})
{
}

LabelFormatter::~LabelFormatter()
{
    // Required here for forward-declared scoped pointer destruction.
}

QString LabelFormatter::formatLabel(
    qint64 timeMs,
    const QTimeZone& timeZone,
    TimelineZoomLevel::LevelType level) const
{
    const auto dt = QDateTime::fromMSecsSinceEpoch(timeMs, timeZone);

    switch (level)
    {
        case TimelineZoomLevel::Years:
            return d->locale.toString(dt, "yyyy");

        case TimelineZoomLevel::Months:
        case TimelineZoomLevel::Days:
        {
            return (dt.date().month() > 1 || dt.date().day() > 1)
                ? d->locale.toString(dt, "d MMM")
                : d->locale.toString(dt, "yyyy\nd MMM");
        }

        case TimelineZoomLevel::Hours:
        case TimelineZoomLevel::Minutes:
        {
            if (d->amPm)
            {
                return (dt.time().hour() > 0 || dt.time().minute() > 0)
                    ? d->locale.toString(dt, "H:mm AP")
                    : d->locale.toString(dt, "d MMM\nH:mm AP");
            }

            return (dt.time().hour() > 0 || dt.time().minute() > 0)
                ? d->locale.toString(dt, "H:mm")
                : d->locale.toString(dt, "d MMM\nH:mm");
        }

        case TimelineZoomLevel::Seconds:
        {
            if (dt.time().second() > 0)
                return nx::format("%1s", dt.time().second());

            return d->amPm
                ? d->locale.toString(dt, "H:mm AP")
                : d->locale.toString(dt, "H:mm");
        }

        case TimelineZoomLevel::Milliseconds:
        {
            return dt.time().msec() > 0
                ? nx::format("%1ms", dt.time().msec())
                : nx::format("%1s", dt.time().second());
        }
    }

    NX_ASSERT(false);
    return {};
}

QString LabelFormatter::formatMarker(
    qint64 timeMs,
    const QTimeZone& timeZone,
    qreal millisecondsPerPixel) const
{
    const auto dt = QDateTime::fromMSecsSinceEpoch(timeMs, timeZone);
    const auto resolution = millisecondsPerPixel * 10;

    constexpr qint64 kDayMs = milliseconds(24h).count();
    constexpr qint64 kMinuteMs = milliseconds(1min).count();
    constexpr qint64 kSecondMs = milliseconds(1s).count();

    if (resolution < kSecondMs)
        return nx::format("%1s", d->locale.toString(dt, "ss.zzz"));

    if (resolution < kMinuteMs)
    {
        return d->amPm
            ? d->locale.toString(dt, "H:mm:ss AP")
            : d->locale.toString(dt, "H:mm:ss");
    }

    if (resolution < kDayMs)
    {
        return d->amPm
            ? d->locale.toString(dt, "H:mm AP")
            : d->locale.toString(dt, "H:mm");
    }

    return d->locale.toString(dt, "d MMM");
}

QString LabelFormatter::formatOutsideMarker(
    qint64 timeMs,
    const QTimeZone& timeZone,
    TimelineZoomLevel::LevelType level) const
{
    const auto dt = QDateTime::fromMSecsSinceEpoch(timeMs, timeZone);
    switch (level)
    {
        case TimelineZoomLevel::Years:
        case TimelineZoomLevel::Months:
        case TimelineZoomLevel::Days:
        case TimelineZoomLevel::Hours:
        {
            return d->amPm
                ? d->locale.toString(dt, "d MMM yyyy, H:mm AP")
                : d->locale.toString(dt, "d MMM yyyy, H:mm");
        }

        case TimelineZoomLevel::Minutes:
        {
            return d->amPm
                ? d->locale.toString(dt, "d MMM yyyy, H:mm:ss AP")
                : d->locale.toString(dt, "d MMM yyyy, H:mm:ss");
        }

        case TimelineZoomLevel::Seconds:
        case TimelineZoomLevel::Milliseconds:
        {
            return d->amPm
                ? d->locale.toString(dt, "d MMM yyyy, H:mm:ss.zzz AP")
                : d->locale.toString(dt, "d MMM yyyy, H:mm:ss.zzz");
        }
    }

    NX_ASSERT(false);
    return {};
}

QString LabelFormatter::formatHeader(
    qint64 startTimeMs, qint64 endTimeMs, const QTimeZone& timeZone) const
{
    const auto startDt = QDateTime::fromMSecsSinceEpoch(startTimeMs, timeZone);
    const auto endDt = QDateTime::fromMSecsSinceEpoch(endTimeMs, timeZone);

    const auto startDate = startDt.date();
    const auto endDate = endDt.date();

    if (startDate.year() != endDate.year())
    {
        return nx::format("%1 - %2",
            d->locale.toString(startDt, "yyyy"), d->locale.toString(endDt, "yyyy"));
    }

    if (startDate.month() != endDate.month())
        d->locale.toString(startDt, "yyyy");

    if (startDate.day() != endDate.day())
        d->locale.toString(startDt, "MMM yyyy");

    const auto startTime = startDt.time();
    const auto endTime = endDt.time();

    if (startTime.hour() != endTime.hour())
        d->locale.toString(startDt, "d MMM yyyy");

    if (startTime.minute() != endTime.minute())
    {
        if (d->amPm)
        {
            return nx::format("%1, %2 - %3", d->locale.toString(startDt, "d MMM yyyy"),
                d->locale.toString(startDt, "HH:mm AP"), d->locale.toString(endDt, "HH:mm AP"));
        }

        return nx::format("%1, %2 - %3", d->locale.toString(startDt, "d MMM yyyy"),
            d->locale.toString(startDt, "HH:mm"), d->locale.toString(endDt, "HH:mm"));
    }

    if (startTime.second() != endTime.second())
    {
        return d->amPm
            ? d->locale.toString(startDt, "d MMM yyyy, HH:mm AP")
            : d->locale.toString(startDt, "d MMM yyyy, HH:mm");
    }

    return d->amPm
        ? d->locale.toString(startDt, "d MMM yyyy, HH:mm:ss AP")
        : d->locale.toString(startDt, "d MMM yyyy, HH:mm:ss");
}

QString LabelFormatter::formatTimestamp(qint64 timestampMs, const QTimeZone& timeZone) const
{
    const auto dt = QDateTime::fromMSecsSinceEpoch(timestampMs, timeZone);
    return d->amPm
        ? d->locale.toString(dt, "d MMM yyyy, H:mm AP")
        : d->locale.toString(dt, "d MMM yyyy, H:mm");
}

QString LabelFormatter::formatCameraTimestamp(qint64 timestampMs, const QTimeZone& timeZone) const
{
    const auto dt = QDateTime::fromMSecsSinceEpoch(timestampMs, timeZone);
    return d->locale.toString(dt, QLocale::ShortFormat);
}

QLocale LabelFormatter::locale() const
{
    return d->locale;
}

void LabelFormatter::setLocale(QLocale value)
{
    if (d->locale == value)
        return;

    d->locale = value;
    d->amPm = d->is12HourFormat();

    emit localeChanged();
}

void LabelFormatter::registerQmlType()
{
    qmlRegisterType<LabelFormatter>("nx.vms.client.core.timeline", 1, 0, "LabelFormatter");
}

} // namespace timeline
} // namespace nx::vms::client::core
