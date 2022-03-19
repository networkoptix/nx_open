// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "frame_time_points_provider_instrument.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QJsonObject>
#include <QtCore/QTimer>

#include <boost/circular_buffer.hpp>

#include <nx/utils/trace/trace.h>
#include <nx/vms/client/desktop/ini.h>

namespace {

static constexpr int64_t kCanaryIntervalMs = 1000;

} // namespace

namespace nx::vms::client::desktop {

struct FrameTimePointsProviderInstrument::Private
{
    Private();

    QElapsedTimer elapsedTimer;

    int64_t delayReferenceTimestampMs;
    QTimer delayMeasurementTimer;

    boost::circular_buffer<qint64> frameTimePoints{
        static_cast<std::size_t>(ini().storeFrameTimePoints)};
};

FrameTimePointsProviderInstrument::Private::Private()
{
    delayMeasurementTimer.setSingleShot(true);
    delayMeasurementTimer.setInterval(kCanaryIntervalMs);
    elapsedTimer.start();
}

FrameTimePointsProviderInstrument::FrameTimePointsProviderInstrument(QObject* parent):
    base_type(Viewport, {QEvent::Paint}, parent),
    d(new Private())
{
    // Trace counter for the measured timer delay. High delay means the UI is lagging.
    connect(&d->delayMeasurementTimer, &QTimer::timeout, this,
        [this]()
        {
            const auto delayMs = d->elapsedTimer.elapsed()
                - d->delayReferenceTimestampMs
                - kCanaryIntervalMs;

            NX_TRACE_COUNTER("Timer").args({{"delay", delayMs}});

            d->delayReferenceTimestampMs = d->elapsedTimer.elapsed();
            d->delayMeasurementTimer.start();
        });

    d->delayReferenceTimestampMs = d->elapsedTimer.elapsed();
    d->delayMeasurementTimer.start();
}

FrameTimePointsProviderInstrument::~FrameTimePointsProviderInstrument()
{
    ensureUninstalled();
}

std::vector<qint64> FrameTimePointsProviderInstrument::getFrameTimePoints()
{
    return std::vector<qint64>(d->frameTimePoints.cbegin(), d->frameTimePoints.cend());
}

bool FrameTimePointsProviderInstrument::paintEvent(QWidget*, QPaintEvent*)
{
    d->frameTimePoints.push_back(d->elapsedTimer.elapsed());
    return false;
}

} // namespace nx::vms::client::desktop
