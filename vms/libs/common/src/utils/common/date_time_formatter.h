#pragma once

#include <QtCore/QDateTime>

class QLocale;

struct QnDateTimeFormatter {
public:
    static QString dateTimeToString(const QString &format, const QDate *date, const QTime *time, const QLocale *locale);

    static QString dateTimeToString(const QString &format, const QDateTime &dateTime, const QLocale &locale) {
        QDate date = dateTime.date();
        QTime time = dateTime.time();
        return dateTimeToString(format, &date, &time, &locale);
    }
};
