// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication>

class NX_VMS_COMMON_API QnTimeStrings
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
    static QString longSuffixCapitalized(Suffix value);
    static QString fullSuffixCapitalized(Suffix value, int count = -1);
};
