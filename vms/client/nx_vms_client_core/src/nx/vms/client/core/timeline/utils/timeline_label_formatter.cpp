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
            return dt.toString("yyyy");

        case TimelineZoomLevel::Months:
        case TimelineZoomLevel::Days:
        {
            return (dt.date().month() > 1 || dt.date().day() > 1)
                ? dt.toString("d MMM")
                : dt.toString("yyyy\nd MMM");
        }

        case TimelineZoomLevel::Hours:
        case TimelineZoomLevel::Minutes:
        {
            return (dt.time().hour() > 0 || dt.time().minute() > 0)
                ? dt.toString("H:mm")
                : dt.toString("d MMM\nH:mm");
        }

        case TimelineZoomLevel::Seconds:
        {
            return dt.time().second() > 0
                ? nx::format("%1s", dt.time().second()).toQString()
                : dt.toString("H:mm");
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
        return nx::format("%1s", dt.time().toString("ss.zzz"));

    if (resolution < kMinuteMs)
        return dt.time().toString("H:mm:ss");

    if (resolution < kDayMs)
        return dt.time().toString("H:mm");

    return dt.date().toString("d MMM");
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
            return dt.toString("d MMM yyyy, H:mm");

        case TimelineZoomLevel::Minutes:
            return dt.toString("d MMM yyyy, H:mm:ss");

        case TimelineZoomLevel::Seconds:
        case TimelineZoomLevel::Milliseconds:
            return dt.toString("d MMM yyyy, H:mm:ss.zzz");
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
        return nx::format("%1 - %2", startDate.toString("yyyy"), endDate.toString("yyyy"));

    if (startDate.month() != endDate.month())
        return startDate.toString("yyyy");

    if (startDate.day() != endDate.day())
        return startDate.toString("MMM yyyy");

    const auto startTime = startDt.time();
    const auto endTime = endDt.time();

    if (startTime.hour() != endTime.hour())
        return startDate.toString("d MMM yyyy");

    if (startTime.minute() != endTime.minute())
    {
        return nx::format("%1, %2 - %3", startDate.toString("d MMM yyyy"),
            startTime.toString("HH:mm"), endTime.toString("HH:mm"));
    }

    if (startTime.second() != endTime.second())
        return startDt.toString("d MMM yyyy, HH:mm");

    return startDt.toString("d MMM yyyy, HH:mm:ss");
}

QString LabelFormatter::formatTimestamp(qint64 timestampMs, const QTimeZone& timeZone) const
{
    const auto dt = QDateTime::fromMSecsSinceEpoch(timestampMs, timeZone);
    return dt.toString("d MMM yyyy, H:mm");
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
    emit localeChanged();
}

void LabelFormatter::registerQmlType()
{
    qmlRegisterType<LabelFormatter>("nx.vms.client.core.timeline", 1, 0, "LabelFormatter");
}

} // namespace timeline
} // namespace nx::vms::client::core
