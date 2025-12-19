// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>
#include <QtCore/QTimeZone>

namespace nx::vms::client::core {

struct NX_VMS_CLIENT_CORE_API TimelineZoomLevel
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

    bool testTick(qint64 tick, const QTimeZone& tz = QTimeZone::LocalTime) const;
    qint64 nextTick(qint64 tick, const QTimeZone& tz = QTimeZone::LocalTime) const;
    qint64 averageTickLength() const;
    qint64 alignTick(qint64 tick, const QTimeZone& tz = QTimeZone::LocalTime) const;
    int tickCount(qint64 start, qint64 end, const QTimeZone& tz = QTimeZone::LocalTime) const;
    bool isMonotonic() const;
    QString value(qint64 tick, const QTimeZone& tz = QTimeZone::LocalTime) const;
    QString suffix(qint64 tick, const QTimeZone& tz = QTimeZone::LocalTime) const;
    QString longestText() const;
};

} // namespace nx::vms::client::core
