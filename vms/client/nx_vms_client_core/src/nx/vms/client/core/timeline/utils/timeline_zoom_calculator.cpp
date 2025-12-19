// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "timeline_zoom_calculator.h"

#include <chrono>

#include <QtQml/QtQml>

#include <nx/utils/math/fuzzy.h>

namespace nx::vms::client::core {
namespace timeline {

namespace {

using namespace std::chrono;

static const std::vector<TimelineZoomLevel> kZoomLevels =
    []()
    {
        std::vector<TimelineZoomLevel> result;
        result.push_back({TimelineZoomLevel::Milliseconds, 10});
        result.push_back({TimelineZoomLevel::Milliseconds, 50});
        result.push_back({TimelineZoomLevel::Milliseconds, 100});
        result.push_back({TimelineZoomLevel::Milliseconds, 500});
        result.push_back({TimelineZoomLevel::Seconds, milliseconds(1s).count()});
        result.push_back({TimelineZoomLevel::Seconds, milliseconds(5s).count()});
        result.push_back({TimelineZoomLevel::Seconds, milliseconds(10s).count()});
        result.push_back({TimelineZoomLevel::Seconds, milliseconds(30s).count()});
        result.push_back({TimelineZoomLevel::Minutes, milliseconds(1min).count()});
        result.push_back({TimelineZoomLevel::Minutes, milliseconds(5min).count()});
        result.push_back({TimelineZoomLevel::Minutes, milliseconds(10min).count()});
        result.push_back({TimelineZoomLevel::Minutes, milliseconds(30min).count()});
        result.push_back({TimelineZoomLevel::Hours, milliseconds(1h).count()});
        result.push_back({TimelineZoomLevel::Hours, milliseconds(3h).count()});
        result.push_back({TimelineZoomLevel::Hours, milliseconds(6h).count()});
        result.push_back({TimelineZoomLevel::Hours, milliseconds(12h).count()});
        result.push_back({TimelineZoomLevel::Days, 1});
        result.push_back({TimelineZoomLevel::Days, 5});
        result.push_back({TimelineZoomLevel::Days, 10});
        result.push_back({TimelineZoomLevel::Months, 1});
        result.push_back({TimelineZoomLevel::Months, 3});
        result.push_back({TimelineZoomLevel::Months, 6});
        result.push_back({TimelineZoomLevel::Years, 1});
        result.push_back({TimelineZoomLevel::Years, 5});
        result.push_back({TimelineZoomLevel::Years, 10});
        result.push_back({TimelineZoomLevel::Years, 50});
        result.push_back({TimelineZoomLevel::Years, 100});
        return result;
    }();

} // namespace

struct ZoomCalculator::Private
{
    ZoomCalculator* const q;

    QTimeZone timeZone{QTimeZone::LocalTime};

    qint64 startTimeMs{0};
    qint64 durationMs{60 * 60 * 1000};
    qreal minimumTickSpacingMs{1000};

    TimelineZoomLevel::LevelType majorTicksLevel{};
    QList<qint64> majorTicks;

    QList<qint64> minorTicks;

    int maximumTickCount = 100;

    void update();
};

// ------------------------------------------------------------------------------------------------
// ZoomCalculator

ZoomCalculator::ZoomCalculator(QObject* parent):
    QObject(parent),
    d(new Private{.q = this})
{
    d->update();
}

ZoomCalculator::~ZoomCalculator()
{
    // Required here for forward-declared scoped pointer destruction.
}

qint64 ZoomCalculator::startTimeMs() const
{
    return d->startTimeMs;
}

void ZoomCalculator::setStartTimeMs(qint64 value)
{
    if (d->startTimeMs == value)
        return;

    d->startTimeMs = value;
    emit startTimeChanged();

    d->update();
}

qint64 ZoomCalculator::durationMs() const
{
    return d->durationMs;
}

void ZoomCalculator::setDurationMs(qint64 value)
{
    if (d->durationMs == value)
        return;

    d->durationMs = value;
    emit durationChanged();

    d->update();
}

qreal ZoomCalculator::minimumTickSpacingMs() const
{
    return d->minimumTickSpacingMs;
}

void ZoomCalculator::setMinimumTickSpacingMs(qreal value)
{
    if (qFuzzyEquals(d->minimumTickSpacingMs, value))
        return;

    d->minimumTickSpacingMs = value;
    emit minimumTickSpacingChanged();

    d->update();
}

TimelineZoomLevel::LevelType ZoomCalculator::majorTicksLevel() const
{
    return d->majorTicksLevel;
}

QList<qint64> ZoomCalculator::majorTicks() const
{
    return d->majorTicks;
}

QList<qint64> ZoomCalculator::minorTicks() const
{
    return d->minorTicks;
}

QTimeZone ZoomCalculator::timeZone() const
{
    return d->timeZone;
}

void ZoomCalculator::setTimeZone(const QTimeZone& value)
{
    if (d->timeZone == value)
        return;

    d->timeZone = value;
    emit timeZoneChanged();

    d->update();
}

void ZoomCalculator::registerQmlType()
{
    qmlRegisterType<ZoomCalculator>("nx.vms.client.core.timeline", 1, 0, "ZoomCalculator");
}

// ------------------------------------------------------------------------------------------------
// ZoomCalculator::Private

void ZoomCalculator::Private::update()
{
    // A safety check.
    // As durationMs and minimumTickSpacingMs can be assigned in arbitrary order, it can lead to
    // very computational-heavy situations.
    if ((durationMs / minimumTickSpacingMs) > maximumTickCount)
    {
        if (!majorTicks.empty())
        {
            majorTicks = {};
            emit q->majorTicksChanged();
        }

        if (!minorTicks.empty())
        {
            minorTicks = {};
            emit q->minorTicksChanged();
        }

        return;
    }

    const auto it = std::upper_bound(kZoomLevels.cbegin(), kZoomLevels.cend(), minimumTickSpacingMs,
        [this](qreal minStep, const TimelineZoomLevel& level)
        {
            return minStep <= level.averageTickLength();
        });

    TimelineZoomLevel minorZoomLevel = it != kZoomLevels.end() ? (*it) : kZoomLevels.back();
    if (it == kZoomLevels.end())
    {
        do { minorZoomLevel.interval *= 10; }
        while(minimumTickSpacingMs > minorZoomLevel.averageTickLength());
    }

    QList<qint64> newMajorTicks;
    QList<qint64> newMinorTicks;

    const auto nextIt = it == kZoomLevels.end() ? it : (it + 1);
    const auto majorZoomLevel = nextIt != kZoomLevels.end()
        ? *nextIt
        : TimelineZoomLevel{minorZoomLevel.type, minorZoomLevel.interval * 10};

    qint64 previousMinorTickMs = minorZoomLevel.alignTick(startTimeMs, timeZone);
    qint64 previousMajorTickMs = majorZoomLevel.alignTick(startTimeMs, timeZone);

    auto minorTickMs = previousMinorTickMs == startTimeMs
        ? previousMinorTickMs
        : minorZoomLevel.nextTick(previousMinorTickMs, timeZone);

    auto majorTickMs = previousMajorTickMs == startTimeMs
        ? previousMajorTickMs
        : majorZoomLevel.nextTick(previousMajorTickMs, timeZone);

    const auto endTimeMs = startTimeMs + durationMs;
    while (minorTickMs <= endTimeMs)
    {
        if (minorTickMs < majorTickMs)
        {
            newMinorTicks.push_back(minorTickMs);
        }
        else
        {
            newMajorTicks.push_back(majorTickMs);
            minorTickMs = majorTickMs;

            previousMajorTickMs = majorTickMs;
            majorTickMs = majorZoomLevel.nextTick(previousMajorTickMs, timeZone);
        }

        previousMinorTickMs = minorTickMs;
        minorTickMs = minorZoomLevel.nextTick(previousMinorTickMs, timeZone);
    }

    if (majorTicks != newMajorTicks)
    {
        majorTicks = newMajorTicks;
        emit q->majorTicksChanged();
    }

    if (majorTicksLevel != majorZoomLevel.type)
    {
        majorTicksLevel = majorZoomLevel.type;
        emit q->majorTicksLevelChanged();
    }

    if (minorTicks != newMinorTicks)
    {
        minorTicks = newMinorTicks;
        emit q->minorTicksChanged();
    }
}

} // namespace timeline
} // namespace nx::vms::client::core
