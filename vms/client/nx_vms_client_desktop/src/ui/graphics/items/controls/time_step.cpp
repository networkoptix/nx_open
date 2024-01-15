// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "time_step.h"

#include <QtCore/QDateTime>

#include <date_time_formatter/date_time_formatter.h>
#include <nx/utils/math/math.h>

using std::chrono::milliseconds;

namespace
{
inline milliseconds timeToMSecs(const QTime& time)
{
    return milliseconds(QTime(0, 0, 0, 0).msecsTo(time));
}

QDateTime addHours(const QDateTime& dateTime, int hours, const QTimeZone& timeZone)
{
    if (!NX_ASSERT(dateTime.isValid()))
        return dateTime;

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
            dateTime.time().msec()),
        timeZone);

    if (result.isValid())
        return result;

    // In the Qt5/Qt6 the resulting date may be invalid when incoming date and time represent a
    // non-existing time point of daylight saving time shift. For example if the clock is shifted
    // 1h forward in the US, the interval 2:00 - 3:00 will not exist. After 1:59 it's be 3:00.
    // If we try to construct QDateTime for 2:00, it'll become invalid and will have a timestamp of
    // 1:00. To overcome this issue, we need to try to add 1 hour more.
    return addHours(dateTime, hours + 1, timeZone);
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

qint64 roundUp(milliseconds msecs, const QnTimeStep& step, const QTimeZone& timeZone)
{
    if (step.isRelative)
        return qCeil(msecs.count(), step.stepMSecs.count());

    QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(msecs.count(), timeZone);
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
                dateTime = addHours(
                    QDateTime(dateTime.date(), QTime(oldHour, 0, 0, 0), timeZone),
                    newHour - oldHour,
                    timeZone);
            }
            break;

        case QnTimeStep::Days:
            if (dateTime.time() != QTime(0, 0, 0, 0)
                || (dateTime.date().day() != 1 && dateTime.date().day() % step.stepUnits != 0))
            {
                int oldDay = dateTime.date().day();
                int newDay = qMin(
                    qCeil(oldDay + 1, step.stepUnits),
                    dateTime.date().daysInMonth() + 1);
                dateTime = QDateTime(
                    dateTime.date().addDays(newDay - oldDay),
                    QTime(0, 0, 0, 0),
                    timeZone);
            }
            break;

        case QnTimeStep::Months:
            if (dateTime.time() != QTime(0, 0, 0, 0) || dateTime.date().day() != 1
                || ((dateTime.date().month() - 1) % step.stepUnits != 0))
            {
                int oldMonth = dateTime.date().month();
                int newMonth = qCeil(oldMonth /* No +1 here as months are numbered from 1. */,
                    step.stepUnits) + 1;
                dateTime = QDateTime(
                    QDate(dateTime.date().year(), dateTime.date().month(), 1)
                        .addMonths(newMonth - oldMonth),
                    QTime(0, 0, 0, 0),
                    timeZone);
            }
            break;

        case QnTimeStep::Years:
            if (dateTime.time() != QTime(0, 0, 0, 0) || dateTime.date().day() != 1
                || dateTime.date().month() != 1 || dateTime.date().year() % step.stepUnits != 0)
            {
                int oldYear = dateTime.date().year();
                int newYear = qCeil(oldYear + 1, step.stepUnits);
                dateTime = QDateTime(
                    QDate(dateTime.date().year(), 1, 1).addYears(newYear - oldYear),
                    QTime(0, 0, 0, 0),
                    timeZone);
            }
            break;

        default:
            NX_ASSERT(false, "Invalid time step type '%1'.", static_cast<int>(step.type));
            break;
    }

    return dateTime.toMSecsSinceEpoch();
}

qint64 add(milliseconds msecs, const QnTimeStep& step, const QTimeZone& timeZone)
{
    if (step.isRelative)
        return (msecs + step.stepMSecs).count();

    switch (step.type)
    {
        case QnTimeStep::Minutes:
        case QnTimeStep::Seconds:
        case QnTimeStep::Milliseconds:
            return (msecs + step.stepMSecs).count();

        case QnTimeStep::Hours:
            return addHours(QDateTime::fromMSecsSinceEpoch(msecs.count(), timeZone),
                step.stepUnits,
                timeZone).toMSecsSinceEpoch();

        case QnTimeStep::Days:
            return QDateTime::fromMSecsSinceEpoch(msecs.count(), timeZone).
                addDays(step.stepUnits).toMSecsSinceEpoch();

        case QnTimeStep::Months:
            return QDateTime::fromMSecsSinceEpoch(msecs.count(), timeZone).
                addMonths(step.stepUnits).toMSecsSinceEpoch();

        case QnTimeStep::Years:
            return QDateTime::fromMSecsSinceEpoch(msecs.count(), timeZone).
                addYears(step.stepUnits).toMSecsSinceEpoch();

        default:
            NX_ASSERT(false, "Invalid time step type '%1'.", static_cast<int>(step.type));
            return msecs.count();
    }
}

qint64 sub(milliseconds msecs, const QnTimeStep& step, const QTimeZone& timeZone)
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
            return addHours(QDateTime::fromMSecsSinceEpoch(msecs.count(), timeZone),
                -step.stepUnits,
                timeZone).toMSecsSinceEpoch();

        case QnTimeStep::Days:
            return QDateTime::fromMSecsSinceEpoch(msecs.count(), timeZone).
                addDays(-step.stepUnits).toMSecsSinceEpoch();

        case QnTimeStep::Months:
            return QDateTime::fromMSecsSinceEpoch(msecs.count(), timeZone).
                addMonths(-step.stepUnits).toMSecsSinceEpoch();

        case QnTimeStep::Years:
            return QDateTime::fromMSecsSinceEpoch(msecs.count(), timeZone).
                addYears(-step.stepUnits).toMSecsSinceEpoch();

        default:
            NX_ASSERT(false, "Invalid time step type '%1'.", static_cast<int>(step.type));
            return msecs.count();
    }
}

qint64 absoluteNumber(milliseconds msecs, const QnTimeStep& step, const QTimeZone& timeZone)
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

    QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(msecs.count(), timeZone);
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

qint32 shortCacheKey(
    milliseconds msecs,
    int height,
    const QnTimeStep& step,
    const QTimeZone& timeZone)
{
    qint32 timeKey;

    if (step.isRelative)
    {
        timeKey = msecs / step.stepMSecs % step.wrapUnits;
    }
    else
    {
        QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(msecs.count(), timeZone);
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

QString toShortString(milliseconds msecs, const QnTimeStep& step, const QTimeZone& timeZone)
{
    if (step.isRelative)
        return QString::number(msecs / step.unitMSecs % step.wrapUnits) + step.format;

    switch(step.type)
    {
        case QnTimeStep::Milliseconds:
        case QnTimeStep::Seconds:
        {
            return QString::number(
                timeToMSecs(
                    QDateTime::fromMSecsSinceEpoch(msecs.count(), timeZone).time())
                / step.unitMSecs % step.wrapUnits) + step.format;
        }

        default:
        {
            return QnDateTimeFormatter::dateTimeToString(step.format,
                QDateTime::fromMSecsSinceEpoch(msecs.count(), timeZone), QLocale());
        }
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

            if (result.contains("yyyy"))
                result.replace("yyyy", "2000");
            else if (result.contains("yy"))
                result.replace("yy", "2000");

            if (result.contains("hh"))
                result.replace("hh", "23");
            else if (result.contains("h"))
                result.replace("h", "23");

            if (result.contains("HH"))
                result.replace("HH", "23");
            else if (result.contains("H"))
                result.replace("H", "23");

            if (result.contains("mm"))
                result.replace("mm", "59");
            else if (result.contains("m"))
                result.replace("m", "59");

            if (result.contains("ss"))
                result.replace("ss", "59");
            else if (result.contains("s"))
                result.replace("s", "59");

            if (result.contains("zzz"))
                result.replace("zzz", "999");
            else if (result.contains("z"))
                result.replace("z", "999");

            if (result.contains("ap"))
                result.replace("ap", longestAmPm());
            else if (result.contains("a"))
                result.replace("a", longestAmPm());

            if (result.contains("AP"))
                result.replace("AP", longestAmPm());
            else if (result.contains("A"))
                result.replace("A", longestAmPm());

            if (result.contains("MMMM"))
                result.replace("MMMM", longestName(1, 12,
                    [](int i){ return QLocale().monthName(i, QLocale::LongFormat); }));
            else if (result.contains("MMM"))
                result.replace("MMM", longestName(1, 12,
                    [](int i){ return QLocale().monthName(i, QLocale::ShortFormat); }));
            else if (result.contains("MM"))
                result.replace("MM", "12");
            else if (result.contains("M"))
                result.replace("M", "12");

            if (result.contains("dddd"))
                result.replace("dddd", longestName(1, 7,
                    [](int i){ return QLocale().dayName(i, QLocale::LongFormat); }));
            else if (result.contains("ddd"))
                result.replace("ddd", longestName(1, 7,
                    [](int i){ return QLocale().dayName(i, QLocale::ShortFormat); }));
            else if (result.contains("dd"))
                result.replace("dd", "29");
            else if (result.contains("d"))
                result.replace("d", "29");

            return result;
        }
    }
}

QString toLongString(milliseconds msecs, const QnTimeStep& step, const QTimeZone& timeZone)
{
    return step.isRelative
        ? QString()
        : QnDateTimeFormatter::dateTimeToString(
            step.longFormat, QDateTime::fromMSecsSinceEpoch(msecs.count(), timeZone), QLocale());
}
