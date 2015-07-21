
#include "time_helper.h"

#include <QDebug>

enum { kMSecFactor = 1000 };

QDateTime rtu::convertDateTime(const QDate &date
    , const QTime &time
    , const QTimeZone &sourceTimeZone
    , const QTimeZone &targetTimeZone)
{
    const qint64 sourceUtcMSecs = msecondsFromEpoch(date, time, sourceTimeZone);
    const QDateTime targetPseudoUtc = convertUtcToTimeZone(sourceUtcMSecs, targetTimeZone);

    qDebug() << "_______________";
    qDebug() << sourceTimeZone << " : " << targetTimeZone;
    qDebug() << targetPseudoUtc;

    return QDateTime(targetPseudoUtc.date(), targetPseudoUtc.time());
}

QDateTime rtu::convertUtcToTimeZone(qint64 utcTimeMs
    , const QTimeZone &timeZone)
{
    const int offsetFromUtc = timeZone.offsetFromUtc(QDateTime::fromMSecsSinceEpoch(utcTimeMs));
    const QDateTime pseudoDateTime = QDateTime::fromMSecsSinceEpoch(
        utcTimeMs +  offsetFromUtc * kMSecFactor, Qt::UTC);
    //TODO: #ynikitenkov are you sure it should work this way?
    return QDateTime(pseudoDateTime.date(), pseudoDateTime.time());
}

qint64 rtu::utcMsFromTimeZone(const QDate &date
    , const QTime &time
    , const QTimeZone &timeZone)
{
    return msecondsFromEpoch(date, time, timeZone);
}

qint64 rtu::msecondsFromEpoch(const QDate &date
    , const QTime &time
    , const QTimeZone &timeZone)
{
    const int offsetFromUtc = timeZone.offsetFromUtc(QDateTime(date, time, Qt::UTC));
    const QDateTime utcTime = QDateTime(date, time, Qt::UTC);
    
    const qint64 result = (utcTime.toMSecsSinceEpoch() - offsetFromUtc * kMSecFactor);

    qDebug() << "\n" << "source: " << date << " " << time << " " << timeZone;
    qDebug() << offsetFromUtc;
    qDebug() << "target: " << result << "\n";
    return result;
}

QString rtu::timeZoneNameWithOffset(const QTimeZone &timeZone, const QDateTime &atDateTime) {
    if (!timeZone.comment().isEmpty()) {
        Q_ASSERT_X(timeZone.comment().contains("UTC"), Q_FUNC_INFO, "We are waiting for the (UTC+03:00) format");
        return timeZone.comment();
    }

    // Constructing format manually
    QString tzTemplate("(%1) %2");
    QString baseName(timeZone.id());      
    return tzTemplate.arg(timeZone.displayName(atDateTime, QTimeZone::OffsetName), baseName);
}
