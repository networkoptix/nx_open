// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "time_step.h"

#include <QtCore/QDateTime>

#include <utils/math/math.h>
#include <date_time_formatter/date_time_formatter.h>

using std::chrono::milliseconds;

namespace
{
inline milliseconds timeToMSecs(const QTime& time)
{
    return milliseconds(QTime(0, 0, 0, 0).msecsTo(time));
}

QDateTime addHours(const QDateTime& dateTime, int hours)
{
    int oldHours = dateTime.time().hour();
    int newHours = oldHours + hours;

    int deltaDays = 0;
    if (newHours >= 24)
    {
        deltaDays = newHours / 24;
        newHours = qMod(newHours, 24);
    }
    else if (newHours < 0)
    {
        deltaDays = newHours / 24 - 1;
        newHours = qMod(newHours, 24);
    }

    const QDateTime result(
        dateTime.date().addDays(deltaDays),
        QTime(newHours,
            dateTime.time().minute(),
            dateTime.time().second(),
            dateTime.time().msec()));

    if (result.isValid())
        return result;

    // In Qt 5 the resulting date may be invalid when incoming date and time represent a
    // non-existing time point of daylight saving time shift. For example if the clock is shifted
    // 1h forward in the US, the interval 2:00 - 3:00 will not exist. After 1:59 it's be 3:00.
    // If we try to construct QDateTime for 2:00, it'll become invalid and will have a timestamp of
    // 1:00. To overcome this issue, we need to try to add 1 hour more.
    return addHours(dateTime, hours + 1);
}
} // namespace

QnTimeStep::QnTimeStep(
    Type type,
    milliseconds unitMSecs,
    int stepUnits,
    int wrapUnits,
    const QString& format,
    const QString& longFormat,
    bool isRelative)
    :
    type(type),
    unitMSecs(unitMSecs),
    stepMSecs(unitMSecs * stepUnits),
    stepUnits(stepUnits),
    wrapUnits(wrapUnits),
    format(format),
    longFormat(longFormat),
    isRelative(isRelative),
    index(0)
{
}

qint64 roundUp(milliseconds msecs, const QnTimeStep& step)
{
    if (step.isRelative)
        return qCeil(msecs.count(), step.stepMSecs.count());

    QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(msecs.count());
    switch(step.type)
    {
        case QnTimeStep::Milliseconds:
        case QnTimeStep::Seconds:
        case QnTimeStep::Minutes:
        {
            milliseconds oldMSecs = timeToMSecs(dateTime.time());
            milliseconds newMSecs = milliseconds(qCeil(oldMSecs.count(), step.stepMSecs.count()));
            return (msecs + (newMSecs - oldMSecs)).count();
        }

        case QnTimeStep::Hours:
            if (dateTime.time().msec() != 0 || dateTime.time().second() != 0
                || dateTime.time().minute() != 0 || dateTime.time().hour() % step.stepUnits != 0)
            {
                int oldHour = dateTime.time().hour();
                int newHour = qCeil(oldHour + 1, step.stepUnits);
                dateTime = addHours(QDateTime(dateTime.date(), QTime(oldHour, 0, 0, 0)), newHour - oldHour);
            }
            break;

        case QnTimeStep::Days:
            if (dateTime.time() != QTime(0, 0, 0, 0)
                || (dateTime.date().day() != 1 && dateTime.date().day() % step.stepUnits != 0))
            {
                int oldDay = dateTime.date().day();
                int newDay = qMin(qCeil(oldDay + 1, step.stepUnits), dateTime.date().daysInMonth() + 1);
                dateTime = QDateTime(dateTime.date().addDays(newDay - oldDay), QTime(0, 0, 0, 0));
            }
            break;

        case QnTimeStep::Months:
            if (dateTime.time() != QTime(0, 0, 0, 0) || dateTime.date().day() != 1
                || ((dateTime.date().month() - 1) % step.stepUnits != 0))
            {
                int oldMonth = dateTime.date().month();
                int newMonth = qCeil(oldMonth /* No +1 here as months are numbered from 1. */,
                    step.stepUnits) + 1;
                dateTime = QDateTime(QDate(dateTime.date().year(), dateTime.date().month(), 1)
                    .addMonths(newMonth - oldMonth), QTime(0, 0, 0, 0));
            }
            break;

        case QnTimeStep::Years:
            if (dateTime.time() != QTime(0, 0, 0, 0) || dateTime.date().day() != 1
                || dateTime.date().month() != 1 || dateTime.date().year() % step.stepUnits != 0)
            {
                int oldYear = dateTime.date().year();
                int newYear = qCeil(oldYear + 1, step.stepUnits);
                dateTime = QDateTime(QDate(dateTime.date().year(), 1, 1)
                    .addYears(newYear - oldYear), QTime(0, 0, 0, 0));
            }
            break;

        default:
            NX_ASSERT(false, "Invalid time step type '%1'.", static_cast<int>(step.type));
            break;
    }

    return dateTime.toMSecsSinceEpoch();
}

qint64 add(milliseconds msecs, const QnTimeStep& step)
{
    if (step.isRelative)
        return (msecs + step.stepMSecs).count();

    switch(step.type)
    {
        case QnTimeStep::Minutes:
        case QnTimeStep::Seconds:
        case QnTimeStep::Milliseconds:
            return (msecs + step.stepMSecs).count();

        case QnTimeStep::Hours:
            return addHours(QDateTime::fromMSecsSinceEpoch(msecs.count()),
                step.stepUnits).toMSecsSinceEpoch();

        case QnTimeStep::Days:
            return QDateTime::fromMSecsSinceEpoch(msecs.count()).
                addDays(step.stepUnits).toMSecsSinceEpoch();

        case QnTimeStep::Months:
            return QDateTime::fromMSecsSinceEpoch(msecs.count()).
                addMonths(step.stepUnits).toMSecsSinceEpoch();

        case QnTimeStep::Years:
            return QDateTime::fromMSecsSinceEpoch(msecs.count()).
                addYears(step.stepUnits).toMSecsSinceEpoch();

        default:
            NX_ASSERT(false, "Invalid time step type '%1'.", static_cast<int>(step.type));
            return msecs.count();
    }
}

qint64 sub(milliseconds msecs, const QnTimeStep& step)
{
    if (step.isRelative)
        return (msecs - step.stepMSecs).count();

    switch(step.type)
    {
        case QnTimeStep::Minutes:
        case QnTimeStep::Seconds:
        case QnTimeStep::Milliseconds:
            return (msecs - step.stepMSecs).count();

        case QnTimeStep::Hours:
            return addHours(QDateTime::fromMSecsSinceEpoch(msecs.count()),
                -step.stepUnits).toMSecsSinceEpoch();

        case QnTimeStep::Days:
            return QDateTime::fromMSecsSinceEpoch(msecs.count()).
                addDays(-step.stepUnits).toMSecsSinceEpoch();

        case QnTimeStep::Months:
            return QDateTime::fromMSecsSinceEpoch(msecs.count()).
                addMonths(-step.stepUnits).toMSecsSinceEpoch();

        case QnTimeStep::Years:
            return QDateTime::fromMSecsSinceEpoch(msecs.count()).
                addYears(-step.stepUnits).toMSecsSinceEpoch();

        default:
            NX_ASSERT(false, "Invalid time step type '%1'.", static_cast<int>(step.type));
            return msecs.count();
    }
}

qint64 absoluteNumber(milliseconds msecs, const QnTimeStep& step)
{
    if (step.isRelative)
        return msecs / step.stepMSecs;

    switch (step.type)
    {
        case QnTimeStep::Milliseconds:
        case QnTimeStep::Seconds:
        case QnTimeStep::Minutes:
            return msecs / step.stepMSecs;
        default:
            break;
    }

    QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(msecs.count());
    switch(step.type)
    {
        case QnTimeStep::Hours:
            return (dateTime.date().toJulianDay() * 24 + dateTime.time().hour()) / step.stepUnits;

        case QnTimeStep::Days:
            return dateTime.date().toJulianDay() / step.stepUnits;

        case QnTimeStep::Months:
        {
            int year, month;
            dateTime.date().getDate(&year, &month, nullptr);
            return (year * 12 + month) / step.stepUnits;
        }

        case QnTimeStep::Years:
            return dateTime.date().year() / step.stepUnits;

        default:
            NX_ASSERT(false, "Invalid time step type '%1'.", static_cast<int>(step.type));
            return 0;
    }
}

qint32 shortCacheKey(milliseconds msecs, int height, const QnTimeStep& step)
{
    qint32 timeKey;

    if (step.isRelative)
    {
        timeKey = msecs / step.stepMSecs % step.wrapUnits;
    }
    else
    {
        QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(msecs.count());
        switch(step.type)
        {
            case QnTimeStep::Milliseconds:
            case QnTimeStep::Seconds:
            case QnTimeStep::Minutes:
            case QnTimeStep::Hours:
                timeKey = timeToMSecs(dateTime.time()) / step.stepMSecs % step.wrapUnits;
                break;

            case QnTimeStep::Days:
                timeKey = dateTime.date().dayOfYear() + (QDate::isLeapYear(dateTime.date().year()) ? 512 : 0);
                break;

            case QnTimeStep::Months:
                timeKey = dateTime.date().month();
                break;

            case QnTimeStep::Years:
                timeKey = dateTime.date().year();
                break;

            default:
                NX_ASSERT(false, "Invalid time step type '%1'.", static_cast<int>(step.type));
                timeKey = 0;
                break;
        }
    }

    return (timeKey << 16) | (height << 8) | (step.index << 1) | (step.isRelative ? 1 : 0);
}

QnTimeStepLongCacheKey longCacheKey(milliseconds msecs, int height, const QnTimeStep& step)
{
    return QnTimeStepLongCacheKey(msecs.count(),
        (height << 8) | (step.index << 1) | (step.isRelative ? 1 : 0));
}

QString toShortString(milliseconds msecs, const QnTimeStep& step)
{
    if (step.isRelative)
        return QString::number(msecs / step.unitMSecs % step.wrapUnits) + step.format;

    switch(step.type)
    {
        case QnTimeStep::Milliseconds:
        case QnTimeStep::Seconds:
            return QString::number(timeToMSecs(QDateTime::fromMSecsSinceEpoch(msecs.count()).time())
                / step.unitMSecs % step.wrapUnits) + step.format;

        default:
            return QnDateTimeFormatter::dateTimeToString(step.format,
                QDateTime::fromMSecsSinceEpoch(msecs.count()), QLocale());
    }
}

template<class Functor>
QString longestName(int min, int max, const Functor& functor)
{
    QString result;
    for (int i = min; i <= max; i++)
    {
        QString name = functor(i);
        if (name.size() > result.size())
            result = name;
    }
    return result;
}

QString longestAmPm()
{
    QString amText = QLocale().amText();
    QString pmText = QLocale().pmText();

    return amText.size() > pmText.size() ? amText : pmText;
}

QString toLongestShortString(const QnTimeStep& step)
{
    if (step.isRelative)
        return QString::number(step.wrapUnits - 1) + step.format;

    switch(step.type)
    {
        case QnTimeStep::Milliseconds:
        case QnTimeStep::Seconds:
            return QString::number(step.wrapUnits - 1) + step.format;

        default:
        {
            QString result = step.format;

            if (result.contains(lit("yyyy")))
                result.replace(lit("yyyy"), lit("2000"));
            else if (result.contains(lit("yy")))
                result.replace(lit("yy"), lit("2000"));

            if (result.contains(lit("hh")))
                result.replace(lit("hh"), lit("23"));
            else if (result.contains(lit("h")))
                result.replace(lit("h"), lit("23"));

            if (result.contains(lit("HH")))
                result.replace(lit("HH"), lit("23"));
            else if (result.contains(lit("H")))
                result.replace(lit("H"), lit("23"));

            if (result.contains(lit("mm")))
                result.replace(lit("mm"), lit("59"));
            else if (result.contains(lit("m")))
                result.replace(lit("m"), lit("59"));

            if (result.contains(lit("ss")))
                result.replace(lit("ss"), lit("59"));
            else if (result.contains(lit("s")))
                result.replace(lit("s"), lit("59"));

            if (result.contains(lit("zzz")))
                result.replace(lit("zzz"), lit("999"));
            else if (result.contains(lit("z")))
                result.replace(lit("z"), lit("999"));

            if (result.contains(lit("ap")))
                result.replace(lit("ap"), longestAmPm());
            else if (result.contains(lit("a")))
                result.replace(lit("a"), longestAmPm());

            if (result.contains(lit("AP")))
                result.replace(lit("AP"), longestAmPm());
            else if (result.contains(lit("A")))
                result.replace(lit("A"), longestAmPm());

            if (result.contains(lit("MMMM")))
                result.replace(lit("MMMM"), longestName(1, 12,
                    [](int i){ return QLocale().monthName(i, QLocale::LongFormat); }));
            else if (result.contains(lit("MMM")))
                result.replace(lit("MMM"), longestName(1, 12,
                    [](int i){ return QLocale().monthName(i, QLocale::ShortFormat); }));
            else if (result.contains(lit("MM")))
                result.replace(lit("MM"), lit("12"));
            else if (result.contains(lit("M")))
                result.replace(lit("M"), lit("12"));

            if (result.contains(lit("dddd")))
                result.replace(lit("dddd"), longestName(1, 7,
                    [](int i){ return QLocale().dayName(i, QLocale::LongFormat); }));
            else if (result.contains(lit("ddd")))
                result.replace(lit("ddd"), longestName(1, 7,
                    [](int i){ return QLocale().dayName(i, QLocale::ShortFormat); }));
            else if (result.contains(lit("dd")))
                result.replace(lit("dd"), lit("29"));
            else if (result.contains(lit("d")))
                result.replace(lit("d"), lit("29"));

            return result;
        }
    }
}

QString toLongString(milliseconds msecs, const QnTimeStep& step)
{
    return step.isRelative
        ? QString()
        : QnDateTimeFormatter::dateTimeToString(
            step.longFormat, QDateTime::fromMSecsSinceEpoch(msecs.count()), QLocale());
}
