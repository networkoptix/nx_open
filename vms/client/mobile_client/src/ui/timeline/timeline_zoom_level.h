// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QNTIMELINEZOOMLEVEL_H
#define QNTIMELINEZOOMLEVEL_H

#include <QtCore/QDateTime>
#include <QtCore/QVector>

struct QnTimelineZoomLevel
{
    enum LevelType
    {
        Milliseconds,
        Seconds,
        Minutes,
        Hours,
        Days,
        Months,
        Years
    };

    LevelType type;
    qint64 interval;

    bool testTick(qint64 tick) const;
    qint64 nextTick(qint64 tick) const;
    qint64 averageTickLength() const;
    qint64 alignTick(qint64 tick) const;
    int tickCount(qint64 start, qint64 end) const;
    bool isMonotonic() const;
    QString value(qint64 tick) const;
    QString suffix(qint64 tick) const;
    QString longestText() const;
};

#endif // QNTIMELINEZOOMLEVEL_H
