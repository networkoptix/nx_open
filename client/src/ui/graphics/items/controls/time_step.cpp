#include "time_step.h"

#include <QtCore/QDateTime>

#include <utils/common/warnings.h>
#include <utils/math/math.h>
#include <utils/common/time.h>

namespace {
    QDateTime addHours(const QDateTime &dateTime, int hours) {
        int oldHours = dateTime.time().hour();
        int newHours = oldHours + hours;

        int deltaDays = 0;
        if(newHours >= 24) {
            deltaDays = newHours / 24;
            newHours = qMod(newHours, 24);
        } else if(newHours < 0) {
            deltaDays = newHours / 24 - 1;
            newHours = qMod(newHours, 24);
        }

        return QDateTime(dateTime.date().addDays(deltaDays), QTime(newHours, dateTime.time().minute(), dateTime.time().second(), dateTime.time().msec()));
    }

} // anonymous namespace

qint64 roundUp(qint64 msecs, const QnTimeStep &step) {
    if(step.isRelative) 
        return qCeil(msecs, step.stepMSecs);
        
    QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(msecs);
    switch(step.type) {
    case QnTimeStep::Milliseconds: {
        qint64 oldMSecs = timeToMSecs(dateTime.time());
        qint64 newMSecs = qCeil(oldMSecs, step.stepMSecs);
        return msecs + (newMSecs - oldMSecs);
    }
    case QnTimeStep::Hours:
        if(dateTime.time().msec() != 0 || dateTime.time().second() != 0 || dateTime.time().minute() != 0 || dateTime.time().hour() % step.stepUnits != 0) {
            int oldHour = dateTime.time().hour();
            int newHour = qCeil(oldHour + 1, step.stepUnits);
            dateTime = addHours(QDateTime(dateTime.date(), QTime(oldHour, 0, 0, 0)), newHour - oldHour);
        }
        break;
    case QnTimeStep::Days:
        if(dateTime.time() != QTime(0, 0, 0, 0) || (dateTime.date().day() != 1 && dateTime.date().day() % step.stepUnits != 0)) {
            int oldDay = dateTime.date().day();
            int newDay = qMin(qCeil(oldDay + 1, step.stepUnits), dateTime.date().daysInMonth() + 1);
            dateTime = QDateTime(dateTime.date().addDays(newDay - oldDay), QTime(0, 0, 0, 0));
        }
        break;
    case QnTimeStep::Months:
        if(dateTime.time() != QTime(0, 0, 0, 0) || dateTime.date().day() != 1 || ((dateTime.date().month() - 1) % step.stepUnits != 0)) {
            int oldMonth = dateTime.date().month();
            int newMonth = qCeil(oldMonth /* No +1 here as months are numbered from 1. */, step.stepUnits) + 1;
            dateTime = QDateTime(QDate(dateTime.date().year(), dateTime.date().month(), 1).addMonths(newMonth - oldMonth), QTime(0, 0, 0, 0));
        }
        break;
    case QnTimeStep::Years:
        if(dateTime.time() != QTime(0, 0, 0, 0) || dateTime.date().day() != 1 || dateTime.date().month() != 1 || dateTime.date().year() % step.stepUnits != 0) {
            int oldYear = dateTime.date().year();
            int newYear = qCeil(oldYear + 1, step.stepUnits);
            dateTime = QDateTime(QDate(dateTime.date().year(), 1, 1).addYears(newYear - oldYear), QTime(0, 0, 0, 0));
        }
        break;
    default:
        qnWarning("Invalid time step type '%1'.", static_cast<int>(step.type));
        break;
    }

    return dateTime.toMSecsSinceEpoch();
}

qint64 add(qint64 msecs, const QnTimeStep &step) {
    if(step.isRelative)
        return msecs + step.stepMSecs;

    switch(step.type) {
    case QnTimeStep::Milliseconds:
        return msecs + step.stepMSecs;
    case QnTimeStep::Hours:
        return addHours(QDateTime::fromMSecsSinceEpoch(msecs), step.stepUnits).toMSecsSinceEpoch();
    case QnTimeStep::Days:
        return QDateTime::fromMSecsSinceEpoch(msecs).addDays(step.stepUnits).toMSecsSinceEpoch();
    case QnTimeStep::Months:
        return QDateTime::fromMSecsSinceEpoch(msecs).addMonths(step.stepUnits).toMSecsSinceEpoch();
    case QnTimeStep::Years:
        return QDateTime::fromMSecsSinceEpoch(msecs).addYears(step.stepUnits).toMSecsSinceEpoch();
    default:
        qnWarning("Invalid time step type '%1'.", static_cast<int>(step.type));
        return msecs;
    }
}

qint64 sub(qint64 msecs, const QnTimeStep &step) {
    if(step.isRelative)
        return msecs - step.stepMSecs;

    switch(step.type) {
    case QnTimeStep::Milliseconds:
        return msecs - step.stepMSecs;
    case QnTimeStep::Hours:
        return addHours(QDateTime::fromMSecsSinceEpoch(msecs), -step.stepUnits).toMSecsSinceEpoch();
    case QnTimeStep::Days:
        return QDateTime::fromMSecsSinceEpoch(msecs).addDays(-step.stepUnits).toMSecsSinceEpoch();
    case QnTimeStep::Months:
        return QDateTime::fromMSecsSinceEpoch(msecs).addMonths(-step.stepUnits).toMSecsSinceEpoch();
    case QnTimeStep::Years:
        return QDateTime::fromMSecsSinceEpoch(msecs).addYears(-step.stepUnits).toMSecsSinceEpoch();
    default:
        qnWarning("Invalid time step type '%1'.", static_cast<int>(step.type));
        return msecs;
    }
}

qint64 absoluteNumber(qint64 msecs, const QnTimeStep &step) {
    if(step.isRelative)
        return msecs / step.stepMSecs;

    if(step.type == QnTimeStep::Milliseconds)
        return msecs / step.stepMSecs;

    QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(msecs);
    switch(step.type) {
    case QnTimeStep::Hours:
        return (dateTime.date().toJulianDay() * 24 + dateTime.time().hour()) / step.stepUnits;
    case QnTimeStep::Days:
        return dateTime.date().toJulianDay() / step.stepUnits;
    case QnTimeStep::Months: {
        int year, month;
        dateTime.date().getDate(&year, &month, NULL);
        return (year * 12 + month) / step.stepUnits;
    }
    case QnTimeStep::Years:
        return dateTime.date().year() / step.stepUnits;
    default:
        qnWarning("Invalid time step type '%1'.", static_cast<int>(step.type));
        return 0;
    }
}

qint32 shortCacheKey(qint64 msecs, int height, const QnTimeStep &step) {
    qint32 timeKey;

    if(step.isRelative) {
        timeKey = msecs / step.stepMSecs % step.wrapUnits;
    } else {
        QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(msecs);
        switch(step.type) {
        case QnTimeStep::Milliseconds:
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
            qnWarning("Invalid time step type '%1'.", static_cast<int>(step.type));
            timeKey = 0;
            break;
        }
    }

    return (timeKey << 16) | (height << 8) | (step.index << 1) | (step.isRelative ? 1 : 0);
}

QnTimeStepLongCacheKey longCacheKey(qint64 msecs, int height, const QnTimeStep &step) {
    return QnTimeStepLongCacheKey(
        msecs,
        (height << 8) | (step.index << 1) | (step.isRelative ? 1 : 0)
    );
}

QString toShortString(qint64 msecs, const QnTimeStep &step) {
    if(step.isRelative)
        return QString::number(msecs / step.unitMSecs % step.wrapUnits) + step.format;

    switch(step.type) {
    case QnTimeStep::Milliseconds:
    case QnTimeStep::Hours:
        return QString::number(timeToMSecs(QDateTime::fromMSecsSinceEpoch(msecs).time()) / step.unitMSecs % step.wrapUnits) + step.format;
    default:
        return QLocale().toString(QDateTime::fromMSecsSinceEpoch(msecs), step.format);
    }
}

QString toLongestShortString(const QnTimeStep &step) {
    if(step.isRelative)
        return QString::number(step.wrapUnits - 1) + step.format;

    switch(step.type) {
    case QnTimeStep::Milliseconds:
    case QnTimeStep::Hours:
        return QString::number(step.wrapUnits - 1) + step.format;
    default:
        return QString();
    }
}

QString toLongString(qint64 msecs, const QnTimeStep &step) {
    if(step.isRelative) {
        return QString();
    } else {
        return QLocale().toString(QDateTime::fromMSecsSinceEpoch(msecs), step.longFormat);
    }
}

