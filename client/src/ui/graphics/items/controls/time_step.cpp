#include "time_step.h"

#include <QtCore/QDateTime>

#include <utils/common/warnings.h>
#include <utils/common/math.h>

namespace {
    qint64 timeToMSecs(const QTime &time) {
        return QTime(0, 0, 0, 0).msecsTo(time);
    }

    QTime msecsToTime(qint64 msecs) {
        return QTime(0, 0, 0, 0).addMSecs(msecs); 
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
        dateTime = dateTime.addMSecs(newMSecs - oldMSecs);
        break;
    }
    case QnTimeStep::Days:
        if(dateTime.time() != QTime(0, 0, 0, 0) || (dateTime.date().day() != 1 && dateTime.date().day() % step.stepUnits != 0)) {
            dateTime.setTime(QTime(0, 0, 0, 0));

            int oldDay = dateTime.date().day();
            int newDay = qMin(qCeil(oldDay + 1, step.stepUnits), dateTime.date().daysInMonth() + 1);
            dateTime = dateTime.addDays(newDay - oldDay);
        }
        break;
    case QnTimeStep::Months:
        if(dateTime.time() != QTime(0, 0, 0, 0) || dateTime.date().day() != 1 || ((dateTime.date().month() - 1) % step.stepUnits != 0)) {
            dateTime.setTime(QTime(0, 0, 0, 0));
            dateTime.setDate(QDate(dateTime.date().year(), dateTime.date().month(), 1));
                
            int oldMonth = dateTime.date().month();
            /* We should have added 1 to month() here we don't want to end
             * up with the same month number, but months are numbered from 1,
             * so the addition is not needed. */
            int newMonth = qCeil(oldMonth, step.stepUnits) + 1;
            dateTime = dateTime.addMonths(newMonth - oldMonth);
        }
        break;
    case QnTimeStep::Years:
        if(dateTime.time() != QTime(0, 0, 0, 0) || dateTime.date().day() != 1 || dateTime.date().month() != 1 || dateTime.date().year() % step.stepUnits != 0) {
            dateTime.setTime(QTime(0, 0, 0, 0));
            dateTime.setDate(QDate(dateTime.date().year(), 1, 1));
                
            int oldYear = dateTime.date().year();
            int newYear = qCeil(oldYear + 1, step.stepUnits);
            dateTime = dateTime.addYears(newYear - oldYear);
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
    case QnTimeStep::Days:
        return msecs + step.stepMSecs;
    case QnTimeStep::Months:
        return QDateTime::fromMSecsSinceEpoch(msecs).addMonths(step.stepUnits).toMSecsSinceEpoch();
    case QnTimeStep::Years:
        return QDateTime::fromMSecsSinceEpoch(msecs).addYears(step.stepUnits).toMSecsSinceEpoch();
    default:
        qnWarning("Invalid time step type '%1'.", static_cast<int>(step.type));
        return msecs;
    }
}

const QDateTime baseDateTime = QDateTime::fromMSecsSinceEpoch(0);

qint64 absoluteNumber(qint64 msecs, const QnTimeStep &step) {
    if(step.isRelative)
        return msecs / step.stepMSecs;

    QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(msecs);
    switch(step.type) {
    case QnTimeStep::Milliseconds:
    case QnTimeStep::Days:
        return baseDateTime.msecsTo(dateTime) / step.stepMSecs;
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

qint32 cacheKey(qint64 msecs, int height, const QnTimeStep &step) {
    qint32 timeKey;

    if(step.isRelative) {
        timeKey = msecs / step.stepMSecs % step.wrapUnits;
    } else {
        QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(msecs);
        switch(step.type) {
        case QnTimeStep::Milliseconds:
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

QString toString(qint64 msecs, const QnTimeStep &step) {
    if(step.isRelative)
        return QString::number(msecs / step.unitMSecs % step.wrapUnits) + step.format;

    if(step.type == QnTimeStep::Milliseconds) {
        return QString::number(timeToMSecs(QDateTime::fromMSecsSinceEpoch(msecs).time()) / step.unitMSecs % step.wrapUnits) + step.format;
    } else {
        return QDateTime::fromMSecsSinceEpoch(msecs).toString(step.format);
    }
}

QString toLongString(qint64 msecs, const QnTimeStep &step) {
    if(step.isRelative) {
        return QString();
    } else {
        return QDateTime::fromMSecsSinceEpoch(msecs).toString(step.longFormat);
    }
}

