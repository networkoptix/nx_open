// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "label_formatter.h"

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

QString LabelFormatter::tickLabel(
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
                    ? d->locale.toString(dt, "h:mm AP")
                    : d->locale.toString(dt, "d MMM\nh:mm AP");
            }

            return (dt.time().hour() > 0 || dt.time().minute() > 0)
                ? d->locale.toString(dt, "h:mm")
                : d->locale.toString(dt, "d MMM\nh:mm");
        }

        case TimelineZoomLevel::Seconds:
        {
            if (dt.time().second() > 0)
                return nx::format("%1s", dt.time().second());

            return d->amPm
                ? d->locale.toString(dt, "h:mm AP")
                : d->locale.toString(dt, "h:mm");
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

QString LabelFormatter::timeMarker(
    qint64 timeMs, const QTimeZone& timeZone, TimelineZoomLevel::LevelType level) const
{
    const auto dt = QDateTime::fromMSecsSinceEpoch(timeMs, timeZone);

    switch (level)
    {
        case TimelineZoomLevel::Years:
            return d->locale.toString(dt, "yyyy");

        case TimelineZoomLevel::Months:
        case TimelineZoomLevel::Days:
            return d->locale.toString(dt, "d MMM");

        case TimelineZoomLevel::Hours:
        case TimelineZoomLevel::Minutes:
        {
            return d->amPm
                ? d->locale.toString(dt, "h:mm AP")
                : d->locale.toString(dt, "h:mm");
        }

        case TimelineZoomLevel::Seconds:
            return nx::format("%1s", d->locale.toString(dt, "s"));

        case TimelineZoomLevel::Milliseconds:
            return nx::format("%1s", d->locale.toString(dt, "s.zzz"));
    }

    NX_ASSERT(false);
    return {};
}

QString LabelFormatter::pressedTimeMarker(qint64 timeMs, const QTimeZone& timeZone,
    qreal millisecondsPerPixel) const
{
    const auto dt = QDateTime::fromMSecsSinceEpoch(timeMs, timeZone);
    const milliseconds resolution{(int) millisecondsPerPixel};

    if (resolution >= 12h)
        return d->locale.toString(dt, "d MMM");

    if (resolution >= 30s)
    {
        return d->amPm
            ? d->locale.toString(dt, "h:mm AP")
            : d->locale.toString(dt, "h:mm");
    }

    if (resolution >= 500ms)
    {
        return d->amPm
            ? d->locale.toString(dt, "h:mm:ss AP")
            : d->locale.toString(dt, "h:mm:ss");
    }

    return nx::format("%1s", d->locale.toString(dt, "s.zzz"));
}

QString LabelFormatter::externalTimeMarker(qint64 timeMs, const QTimeZone& timeZone) const
{
    const auto dt = QDateTime::fromMSecsSinceEpoch(timeMs, timeZone);
    return d->amPm
        ? d->locale.toString(dt, "d MMM yyyy, h:mm:ss AP")
        : d->locale.toString(dt, "d MMM yyyy, h:mm:ss");
}

QString LabelFormatter::windowHeader(
    qint64 startTimeMs, qint64 endTimeMs, const QTimeZone& timeZone) const
{
    const auto startDt = QDateTime::fromMSecsSinceEpoch(startTimeMs, timeZone);
    const auto endDt = QDateTime::fromMSecsSinceEpoch(endTimeMs, timeZone);

    const auto startDate = startDt.date();
    const auto endDate = endDt.date();

    if (milliseconds(endTimeMs - startTimeMs) > 36 * 24h /* approx. 1 month + 20% */)
    {
        if (startDate.year() != endDate.year())
        {
            return nx::format("%1 - %2",
                d->locale.toString(startDt, "yyyy"), d->locale.toString(endDt, "yyyy"));
        }

        return d->locale.toString(startDt, "yyyy");
    }

    if (milliseconds(endTimeMs - startTimeMs) > 30h /* approx. 1 day + 20% */)
    {
        if (startDate.year() != endDate.year())
        {
            return nx::format("%1 - %2", d->locale.toString(startDt, "MMM yyyy"),
                d->locale.toString(endDt, "MMM yyyy"));
        }

        if (startDate.month() != endDate.month())
        {
            return nx::format("%1 - %2", d->locale.toString(startDt, "MMM"),
                d->locale.toString(endDt, "MMM yyyy"));
        }

        return d->locale.toString(startDt, "MMM yyyy");
    }

    if (milliseconds(endTimeMs - startTimeMs) > 72s /* approx. 1 min + 20% */)
    {
        if (startDate.year() != endDate.year())
        {
            return nx::format("%1 - %2", d->locale.toString(startDt, "d MMM yyyy"),
                d->locale.toString(endDt, "d MMM yyyy"));
        }

        if (startDate.month() != endDate.month())
        {
            return nx::format("%1 - %2", d->locale.toString(startDt, "d MMM"),
                d->locale.toString(endDt, "d MMM yyyy"));
        }

        if (startDate.day() != endDate.day())
        {
            return nx::format("%1 - %2", d->locale.toString(startDt, "d"),
                d->locale.toString(endDt, "d MMM yyyy"));
        }

        return d->locale.toString(startDt, "d MMM yyyy");
    }

    if (startDate.year() != endDate.year())
    {
        if (d->amPm)
        {
            return nx::format("%1 - %2", d->locale.toString(startDt, "h:mm AP, d MMM yyyy"),
                d->locale.toString(endDt, "h:mm AP, d MMM yyyy"));
        }

        return nx::format("%1 - %2", d->locale.toString(startDt, "h:mm, d MMM yyyy"),
            d->locale.toString(endDt, "h:mm, d MMM yyyy"));
    }

    if (startDate.month() != endDate.month() || startDate.day() != endDate.day())
    {
        if (d->amPm)
        {
            return nx::format("%1 - %2", d->locale.toString(startDt, "h:mm AP, d MMM"),
                d->locale.toString(endDt, "h:mm AP, d MMM yyyy"));
        }

        return nx::format("%1 - %2", d->locale.toString(startDt, "h:mm, d MMM"),
            d->locale.toString(endDt, "h:mm, d MMM yyyy"));
    }

    const auto startTime = startDt.time();
    const auto endTime = endDt.time();

    if (startTime.hour() != startTime.hour() || startTime.minute() != endTime.minute())
    {
        if (d->amPm)
        {
            return nx::format("%1 - %2", d->locale.toString(startDt, "h:mm AP"),
                d->locale.toString(endDt, "h:mm AP, d MMM yyyy"));
        }

        return nx::format("%1 - %2", d->locale.toString(startDt, "h:mm"),
            d->locale.toString(endDt, "h:mm, d MMM yyyy"));
    }

    return d->amPm
        ? d->locale.toString(endDt, "h:mm AP, d MMM yyyy")
        : d->locale.toString(endDt, "h:mm, d MMM yyyy");
}

QString LabelFormatter::objectTimestamp(qint64 timestampMs, const QTimeZone& timeZone) const
{
    const auto dt = QDateTime::fromMSecsSinceEpoch(timestampMs, timeZone);
    return d->amPm
        ? d->locale.toString(dt, "d MMM yyyy, h:mm AP")
        : d->locale.toString(dt, "d MMM yyyy, h:mm");
}

QString LabelFormatter::cameraTimestamp(qint64 timestampMs, const QTimeZone& timeZone) const
{
    if (timestampMs == -1)
        return {};

    const auto dt = QDateTime::fromMSecsSinceEpoch(timestampMs, timeZone);
    const auto text = d->amPm
        ? d->locale.toString(dt, "d MMM yy h:mm:ss AP")
        : d->locale.toString(dt, "d MMM yy h:mm:ss");

    return text.toUpper();
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
