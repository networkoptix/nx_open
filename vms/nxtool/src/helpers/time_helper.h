
#pragma once

#include <memory>
#include <QDateTime>
#include <QTimeZone>

namespace rtu
{
    QDateTime convertDateTime(const QDate &date
        , const QTime &time
        , const QTimeZone &sourceTimeZone
        , const QTimeZone &targetTimeZone);

    QDateTime convertUtcToTimeZone(qint64 utcTimeMs
        , const QTimeZone &timeZone);

    qint64 msecondsFromEpoch(const QDate &date
        , const QTime &time
        , const QTimeZone &timeZone);

    QString timeZoneNameWithOffset(const QTimeZone &timeZone, const QDateTime &atDateTime);

    struct DateTime
    {
        qint64 utcDateTimeMs;
        QByteArray timeZoneId;

        DateTime(qint64 utcDateTimeMs
            , const QByteArray &timeZoneId);
    };

    typedef std::shared_ptr<DateTime> DateTimePointer;
}
