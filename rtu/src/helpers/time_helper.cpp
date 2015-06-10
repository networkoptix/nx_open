
#include "time_helper.h"

#include <QDebug>

enum { kMSecFactor = 1000 };

QDateTime rtu::convertDateTime(const QDate &date
    , const QTime &time
    , const QTimeZone &sourceTimeZone
    , const QTimeZone &targetTimeZone)
{
    const qint64 sourceUtcMSecs = msecondsFromEpoch(date, time, sourceTimeZone);
    const QDateTime targetPseudoUtc = convertUtcToTimeZone(
        QDateTime::fromMSecsSinceEpoch(sourceUtcMSecs, Qt::UTC), targetTimeZone);

    qDebug() << "_______________";
    qDebug() << sourceTimeZone << " : " << targetTimeZone;
    qDebug() << targetPseudoUtc;

    return QDateTime(targetPseudoUtc.date(), targetPseudoUtc.time());
}

QDateTime rtu::convertUtcToTimeZone(const QDateTime &utcTime
    , const QTimeZone &timeZone)
{
    const int offsetFromUtc = timeZone.offsetFromUtc(utcTime);
    const QDateTime pseudoDateTime = QDateTime::fromMSecsSinceEpoch(
        utcTime.toMSecsSinceEpoch()- offsetFromUtc * kMSecFactor, Qt::UTC);
    return QDateTime(pseudoDateTime.date(), pseudoDateTime.time());
}

QDateTime rtu::utcFromTimeZone(const QDate &date
    , const QTime &time
    , const QTimeZone &timeZone)
{
    return QDateTime::fromMSecsSinceEpoch(
        msecondsFromEpoch(date, time, timeZone), Qt::UTC);
}

qint64 rtu::msecondsFromEpoch(const QDate &date
    , const QTime &time
    , const QTimeZone &timeZone)
{
    const int offsetFromUtc = timeZone.offsetFromUtc(QDateTime(date, time));
    const QDateTime utcTime = QDateTime(date, time, Qt::UTC);

    const qint64 result = (utcTime.toMSecsSinceEpoch() + offsetFromUtc * kMSecFactor);

    qDebug() << "\n" << "source: " << date << " " << time << " " << timeZone;
    qDebug() << "target: " << result << "\n";
    return result;
}
