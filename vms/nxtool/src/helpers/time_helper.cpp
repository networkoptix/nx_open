#include "time_helper.h"

enum { kMSecFactor = 1000 };

QDateTime rtu::convertDateTime(const QDate &date
    , const QTime &time
    , const QTimeZone &sourceTimeZone
    , const QTimeZone &targetTimeZone)
{
    const qint64 sourceUtcMSecs = msecondsFromEpoch(date, time, sourceTimeZone);
    const QDateTime targetPseudoUtc = convertUtcToTimeZone(sourceUtcMSecs, targetTimeZone);

    return QDateTime(targetPseudoUtc.date(), targetPseudoUtc.time());
}

QDateTime rtu::convertUtcToTimeZone(qint64 utcTimeMs
    , const QTimeZone &timeZone)
{
    const int offsetFromUtc = timeZone.offsetFromUtc(QDateTime::fromMSecsSinceEpoch(utcTimeMs));
    const QDateTime pseudoDateTime = QDateTime::fromMSecsSinceEpoch(
        utcTimeMs +  offsetFromUtc * kMSecFactor, Qt::UTC);
    // TODO: #ynikitenkov are you sure it should work this way?
    return QDateTime(pseudoDateTime.date(), pseudoDateTime.time());
}

qint64 rtu::msecondsFromEpoch(const QDate &date
    , const QTime &time
    , const QTimeZone &timeZone)
{
    const int offsetFromUtc = timeZone.offsetFromUtc(QDateTime(date, time, Qt::UTC));
    const QDateTime utcTime = QDateTime(date, time, Qt::UTC);
    
    const qint64 result = (utcTime.toMSecsSinceEpoch() - offsetFromUtc * kMSecFactor);
    return result;
}

QString rtu::timeZoneNameWithOffset(const QTimeZone &timeZone, const QDateTime &atDateTime) {
    if (!timeZone.comment().isEmpty() && timeZone.comment().contains("UTC"))
        return timeZone.comment();

    // Constructing format manually
    static const QString tzTemplate("(%1) %2");
    const QString baseName(timeZone.id());      
    return tzTemplate.arg(timeZone.displayName(atDateTime, QTimeZone::OffsetName), baseName);
}

rtu::DateTime::DateTime(qint64 utcDateTimeMs
    , const QByteArray &timeZoneId)

    : utcDateTimeMs(utcDateTimeMs)
    , timeZoneId(timeZoneId)
{
}
