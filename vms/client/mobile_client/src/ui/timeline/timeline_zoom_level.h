#ifndef QNTIMELINEZOOMLEVEL_H
#define QNTIMELINEZOOMLEVEL_H

#include <QtCore/QDateTime>
#include <QtCore/QVector>

struct QnTimelineZoomLevel {
    enum LevelType {
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
    QString suffixOverride;

    QnTimelineZoomLevel() {}

    QnTimelineZoomLevel(LevelType type, qint64 interval, const QString &suffix = QString()) :
        type(type),
        interval(interval),
        suffixOverride(suffix)
    {}

    bool testTick(qint64 tick) const;
    qint64 nextTick(qint64 tick) const;
    qint64 averageTickLength() const;
    qint64 alignTick(qint64 tick) const;
    QString suffix(qint64 tick) const;
    int tickCount(qint64 start, qint64 end) const;
    bool isMonotonic() const;
    QString baseValue(qint64 tick) const;
    QString subValue(qint64 tick) const;
    QString longestText() const;
};

#endif // QNTIMELINEZOOMLEVEL_H
