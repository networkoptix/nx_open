// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "performance_info.h"

#include <chrono>
#include <thread>

#include <QtCore/QJsonObject>

#include <client/client_module.h>
#include <nx/build_info.h>
#include <nx/utils/trace/trace.h>
#include <nx/vms/client/desktop/debug_utils/components/debug_info_storage.h>
#include <nx/vms/client/desktop/debug_utils/utils/performance_monitor.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/text/human_readable.h>

using namespace std::chrono;
using namespace nx::vms::text;

namespace nx::vms::client::desktop {

namespace {

static constexpr int kMaxThreadCount = 250; //< More is considered as suspicious.

} // namespace

class PerformanceInfo::Private
{
public:
    QString text;

    quint32 frameCount = 0;
    QElapsedTimer fpsTimer;

    std::tuple<qreal, milliseconds> calculateFpsValues()
    {
        const qreal fpsValue = 1000.0 * (qreal) frameCount / (qreal) fpsTimer.elapsed();
        const milliseconds frameTime(frameCount > 0 ? fpsTimer.elapsed() / frameCount : 0);

        frameCount = 0;
        fpsTimer.restart();

        return { fpsValue, frameTime };
    }
};

PerformanceInfo::PerformanceInfo(QObject* parent):
    base_type(parent),
    d(new Private())
{
    connect(qnClientModule->performanceMonitor(), &PerformanceMonitor::valuesChanged,
        this, &PerformanceInfo::setPerformanceValues);
    connect(qnClientModule->performanceMonitor(), &PerformanceMonitor::visibleChanged,
        this, &PerformanceInfo::visibleChanged);

    d->fpsTimer.start();
}

PerformanceInfo::~PerformanceInfo()
{
}

void PerformanceInfo::registerFrame()
{
    ++d->frameCount;
}

void PerformanceInfo::setPerformanceValues(const QVariantMap& values)
{
    using namespace nx::vms::common;

    // On each values update also calculate current FPS.
    const auto [fpsValue, frameTime] = d->calculateFpsValues();

    NX_TRACE_COUNTER_ID("FPS", (int64_t) this).args({{ "FPS", fpsValue }});

    // Create rich text with all values.
    auto remaining = values;
    const QVariant cpuValue = remaining.take(PerformanceMonitor::kCpu);
    const QVariant memoryValue = remaining.take(PerformanceMonitor::kMemory);
    const QVariant gpuValue = remaining.take(PerformanceMonitor::kGpu);
    const QVariant threadsValue = remaining.take(PerformanceMonitor::kThreads);

    const QString memoryText = HumanReadable::digitalSizePrecise(
        memoryValue.toULongLong(),
        HumanReadable::FileSize,
        HumanReadable::DigitalSizeMultiplier::Binary,
        HumanReadable::SuffixFormat::Long,
        ".",
        /*precision*/ 2);

    const QString threadsText = threadsValue.toUInt() > kMaxThreadCount
        ? setWarningStyleHtml(threadsValue.toString())
        : threadsValue.toString();

    QStringList counters;

    // Show main values.
    counters << QString("FPS: %1 (%2ms)")
        .arg(fpsValue, 0, 'f', 1)
        .arg(duration_cast<milliseconds>(frameTime).count());

    counters << QString("CPU: %1%").arg(100.0 * cpuValue.toFloat(), 0, 'f', 1);
    counters << QString("Memory: %1").arg(memoryText);
    if (nx::build_info::isWindows() || nx::build_info::isMacOsX())
        counters << QString("GPU: %1%").arg(100.0 * gpuValue.toFloat(), 0, 'f', 1);
    counters << QString("Threads: %1").arg(threadsText);

    // Show all other values.
    for (auto i = remaining.constBegin(); i != remaining.constEnd(); ++i)
        counters << QString("%1: %2").arg(i.key()).arg(i.value().toString());

    if (qnClientModule->performanceMonitor()->isDebugInfoVisible())
        counters << DebugInfoStorage::instance()->debugInfo();

    d->text = html::document(counters.join(html::kLineBreak));

    emit textChanged(d->text);
}

QString PerformanceInfo::text() const
{
    return d->text;
}

bool PerformanceInfo::isVisible() const
{
    return qnClientModule->performanceMonitor()->isVisible();
}

} // namespace nx::vms::client::desktop
