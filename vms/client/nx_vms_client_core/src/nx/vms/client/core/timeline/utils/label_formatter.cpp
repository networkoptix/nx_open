// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "label_formatter.h"

#include <QtQml/QtQml>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/format.h>

namespace nx::vms::client::core {
namespace timeline {

using namespace std::chrono;

static QString isolateLtr(const QString& source)
{
    return QStringView(u"\u2066") + source + QStringView(u"\u2069"); //< LRI ... PDI.
}

struct LabelFormatter::Private
{
    QLocale locale = QLocale::system();
    QLocale timeLocale{"C"};

    bool amPm = false;
    bool lowerCaseAmPm = false;
    bool rtl = false;
    QString hoursMinutesFormat;
    QString hoursMinutesSecondsFormat;

    void updateLocaleParameters()
    {
        amPm = locale.timeFormat().contains(u"AP"_qs, Qt::CaseInsensitive);
        lowerCaseAmPm = !locale.amText().isUpper();
        rtl = locale.textDirection() == Qt::RightToLeft;

        hoursMinutesFormat = amPm
            ? (lowerCaseAmPm ? u"h:mm ap"_qs : u"h:mm AP"_qs)
            : u"HH:mm"_qs;

        hoursMinutesSecondsFormat = amPm
            ? (lowerCaseAmPm ? u"h:mm:ss ap"_qs : u"h:mm:ss AP"_qs)
            : u"HH:mm:ss"_qs;
    }

    QString year(const QDateTime& dt) const
    {
        return locale.toString(dt, u"yyyy"_qs);
    }

    QString month(const QDateTime& dt) const
    {
        return locale.toString(dt, u"MMM"_qs);
    }

    QString monthYear(const QDateTime& dt) const
    {
        return locale.toString(dt, u"MMM yyyy"_qs);
    }

    QString day(const QDateTime& dt) const
    {
        return locale.toString(dt, u"d"_qs);
    }

    QString dayMonth(const QDateTime& dt) const
    {
        return locale.toString(dt, u"d MMM"_qs);
    }

    QString dayMonthYear(const QDateTime& dt) const
    {
        return locale.toString(dt, u"d MMM yyyy"_qs);
    }

    QString dayMonthShortYear(const QDateTime& dt) const
    {
        return locale.toString(dt, u"d MMM yy"_qs);
    }

    QString hoursMinutes(const QDateTime& dt) const
    {
        return timeLocale.toString(dt, hoursMinutesFormat);
    }

    QString hoursMinutesSeconds(const QDateTime& dt) const
    {
        return timeLocale.toString(dt, hoursMinutesSecondsFormat);
    }
};

LabelFormatter::LabelFormatter(QObject* parent):
    QObject(parent),
    d(new Private{})
{
    d->updateLocaleParameters();
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
            return d->year(dt);

        case TimelineZoomLevel::Months:
        case TimelineZoomLevel::Days:
        {
            if (dt.date().month() > 1 || dt.date().day() > 1)
                return d->dayMonth(dt);

            return nx::format(u"%1\n%2"_qs, d->year(dt), d->dayMonth(dt));
        }

        case TimelineZoomLevel::Hours:
        case TimelineZoomLevel::Minutes:
        {
            if (dt.time().hour() > 0 || dt.time().minute() > 0)
                return d->hoursMinutes(dt);

            return nx::format(u"%1\n%2"_qs, d->dayMonth(dt), isolateLtr(d->hoursMinutes(dt)));
        }

        case TimelineZoomLevel::Seconds:
        {
            if (dt.time().second() > 0)
                return nx::format(u"%1s"_qs, dt.time().second());

            return d->hoursMinutes(dt);
        }

        case TimelineZoomLevel::Milliseconds:
        {
            if (dt.time().msec() > 0)
                return nx::format(u"%1ms"_qs, dt.time().msec());

            return nx::format(u"%1s"_qs, dt.time().second());
        }
    }

    NX_ASSERT(false);
    return {};
}

QString LabelFormatter::htmlTimeMarker(qint64 timeMs, const QTimeZone& timeZone,
    QColor primaryColor, QColor secondaryColor) const
{
    static const auto kTwoLineTemplate =
        u"<p style='margin-right: %3px; line-height: %4px;'>%1<br>%2</p>"_qs;

    static const auto kDateTemplate =
        u"<span style='font-size: 11px; color: %2;'>%1</span>"_qs;

    static const auto kTimeTemplate =
        u"<span style='font-size: %3px; font-weight: 500; color: %2;'>%1</span>"_qs;

    const auto dt = QDateTime::fromMSecsSinceEpoch(timeMs, timeZone);

    const auto timeStr = d->amPm
         ? nx::format(d->timeLocale.toString(dt, u"h:mm:ss %1AP%2"_qs),
             u"<span style='font-size: 8px;'>"_qs,
             u"</span>"_qs).toQString()
         : d->timeLocale.toString(dt, u"HH:mm:ss"_qs);

    return nx::format(kTwoLineTemplate,
        nx::format(kTimeTemplate, isolateLtr(timeStr), primaryColor.name(), d->amPm ? 12 : 14),
        nx::format(kDateTemplate, d->dayMonthShortYear(dt), secondaryColor.name()),
        /*right padding*/ d->amPm ? 2 : 4,
        /*line height*/ d->amPm ? 14 : 11);
}

QString LabelFormatter::externalTimeMarker(qint64 timeMs, const QTimeZone& timeZone) const
{
    const auto dt = QDateTime::fromMSecsSinceEpoch(timeMs, timeZone);
    return nx::format(u"%1, %2"_qs, d->dayMonthYear(dt), d->hoursMinutesSeconds(dt));
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
            return nx::format(u"%1 - %2"_qs, d->year(startDt), d->year(endDt));

        return d->year(startDt);
    }

    if (milliseconds(endTimeMs - startTimeMs) > 30h /* approx. 1 day + 20% */)
    {
        if (startDate.year() != endDate.year())
            return nx::format(u"%1 - %2"_qs, d->monthYear(startDt), d->monthYear(endDt));

        if (startDate.month() != endDate.month())
            return nx::format(u"%1 - %2"_qs, d->month(startDt), d->monthYear(endDt));

        return d->monthYear(startDt);
    }

    if (milliseconds(endTimeMs - startTimeMs) > 72s /* approx. 1 min + 20% */)
    {
        if (startDate.year() != endDate.year())
            return nx::format(u"%1 - %2"_qs, d->dayMonthYear(startDt), d->dayMonthYear(endDt));

        if (startDate.month() != endDate.month())
            return nx::format(u"%1 - %2"_qs, d->dayMonth(startDt), d->dayMonthYear(endDt));

        if (startDate.day() != endDate.day())
            return nx::format(u"%1 - %2"_qs, d->day(startDt), d->dayMonthYear(endDt));

        return d->dayMonthYear(startDt);
    }

    if (startDate.year() != endDate.year())
    {
        return nx::format(u"%1, %2 - %3, %4"_qs, d->hoursMinutes(startDt), d->dayMonthYear(startDt),
            d->hoursMinutes(endDt), d->dayMonthYear(endDt));
    }

    if (startDate.month() != endDate.month() || startDate.day() != endDate.day())
    {
        return nx::format(u"%1, %2 - %3, %4"_qs, d->hoursMinutes(startDt), d->dayMonth(startDt),
            d->hoursMinutes(endDt), d->dayMonthYear(endDt));
    }

    const auto startTime = startDt.time();
    const auto endTime = endDt.time();

    if (startTime.hour() != endTime.hour() || startTime.minute() != endTime.minute())
    {
        return nx::format(u"%1 - %2, %3"_qs, d->hoursMinutes(startDt),
            d->hoursMinutes(endDt), d->dayMonthYear(endDt));
    }

    return nx::format(u"%1, %2"_qs, d->hoursMinutes(startDt), d->dayMonthYear(startDt));
}

QString LabelFormatter::objectTimestamp(qint64 timestampMs, const QTimeZone& timeZone) const
{
    const auto dt = QDateTime::fromMSecsSinceEpoch(timestampMs, timeZone);
    return nx::format(u"%1, %2"_qs, d->dayMonthYear(dt), d->hoursMinutes(dt));
}

QString LabelFormatter::cameraTimestamp(qint64 timestampMs, const QTimeZone& timeZone) const
{
    if (timestampMs == -1)
        return {};

    const auto dt = QDateTime::fromMSecsSinceEpoch(timestampMs, timeZone);
    return nx::format(u"%1 %2"_qs,
        d->dayMonthShortYear(dt), d->hoursMinutesSeconds(dt)).toQString().toUpper();
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
    d->updateLocaleParameters();

    emit localeChanged();
}

bool LabelFormatter::isAmPm() const
{
    return d->amPm;
}

bool LabelFormatter::isRtl() const
{
    return d->rtl;
}

void LabelFormatter::registerQmlType()
{
    qmlRegisterType<LabelFormatter>("nx.vms.client.core.timeline", 1, 0, "LabelFormatter");
}

} // namespace timeline
} // namespace nx::vms::client::core
