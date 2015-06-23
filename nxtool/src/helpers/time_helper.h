
#pragma once

#include <QDateTime>
#include <QTimeZone>

namespace rtu
{
    QDateTime convertDateTime(const QDate &date
        , const QTime &time
        , const QTimeZone &sourceTimeZone
        , const QTimeZone &targetTimeZone);

    QDateTime convertUtcToTimeZone(const QDateTime &utcTime
        , const QTimeZone &timeZone);

    QDateTime utcFromTimeZone(const QDate &date
        , const QTime &time
        , const QTimeZone &timeZone);

    qint64 msecondsFromEpoch(const QDate &date
        , const QTime &time
        , const QTimeZone &timeZone);
}
