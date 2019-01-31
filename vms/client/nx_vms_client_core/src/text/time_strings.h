#pragma once

#include <QtCore/QCoreApplication>

class QnTimeStrings
{
    Q_DECLARE_TR_FUNCTIONS(QnTimeStrings)
public:
    enum class Suffix
    {
        Milliseconds,
        Seconds,
        Minutes,
        Hours,
        Days,
        Weeks,
        Months,
        Years
    };

    static QString suffix(Suffix value);
    static QString longSuffix(Suffix value);
    static QString fullSuffix(Suffix value, int count = -1);

};
