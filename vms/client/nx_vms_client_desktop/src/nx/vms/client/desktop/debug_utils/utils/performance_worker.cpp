// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "performance_worker.h"

#include "performance_monitor.h"

#include <thread>

#include <QtCore/QVariantMap>
#include <QtCore/QJsonObject>
#include <QtCore/QTimer>

#include <nx/monitoring/activity_monitor.h>
#include <nx/utils/trace/trace_categories.h>

#if defined(Q_OS_WIN)
    #include <windows.h>
    #include <tlhelp32.h>
#endif

namespace nx::vms::client::desktop {

PerformanceWorker::PerformanceWorker(QObject* parent):
    base_type(parent),
    m_monitor(monitoring::ActivityMonitor::createForCurrentPlatform())
{
}

PerformanceWorker::~PerformanceWorker()
{
}

void PerformanceWorker::gatherValues()
{
    // This method works in its own thread!

    const qreal processCpuValue = m_monitor->thisProcessCpuUsage();
    const qreal totalCpuValue = m_monitor->totalCpuUsage();
    const quint64 processMemoryValue = m_monitor->thisProcessRamUsageBytes();
    const quint64 processPrivateMemoryValue = m_monitor->thisProcessPrivateRamUsageBytes();
    const auto threadCount = m_monitor->thisProcessThreads();
    const auto processGpuValue = m_monitor->thisProcessGpuUsage();

    QVariantMap values;
    values[PerformanceMonitor::kProcessCpu] = processCpuValue;
    values[PerformanceMonitor::kTotalCpu] = totalCpuValue;
    values[PerformanceMonitor::kMemory] = processMemoryValue;
    values[PerformanceMonitor::kThreads] = threadCount;
    values[PerformanceMonitor::kGpu] = processGpuValue;

    #if defined(Q_OS_WIN)
        const auto processHandle = GetCurrentProcess();
        const quint32 gdiHandleCount = GetGuiResources(processHandle, GR_GDIOBJECTS);
        const quint32 userHandleCount = GetGuiResources(processHandle, GR_USEROBJECTS);

        values["Objects GDI"] = gdiHandleCount;
        values["Objects USER"] = userHandleCount;

        TRACE_COUNTER("resources", "GDI handles", (qint64) gdiHandleCount);
        TRACE_COUNTER("resources", "USER handles", (qint64) userHandleCount);
    #endif

    static constexpr qreal kBytesInMB = 1024.0 * 1024.0;

    // Counters are stacked, so put each counter under its own name.
    TRACE_COUNTER("resources", perfetto::CounterTrack("Process CPU", "%"), processCpuValue * 100);
    TRACE_COUNTER("resources", perfetto::CounterTrack("Total CPU", "%"), totalCpuValue * 100);
    TRACE_COUNTER("resources",
        perfetto::CounterTrack("Process private memory", "MB"),
        (qreal) processPrivateMemoryValue / kBytesInMB);
    TRACE_COUNTER("resources",
        perfetto::CounterTrack("Process memory", "MB"),
        (qreal) processMemoryValue / kBytesInMB);
    TRACE_COUNTER("resources",
        perfetto::CounterTrack("Total memory used", "MB"),
        (qreal) m_monitor->totalRamUsageBytes() / kBytesInMB);
    TRACE_COUNTER("resources", "Threads", threadCount);
    TRACE_COUNTER("resources", perfetto::CounterTrack("Process GPU", "%"), processGpuValue * 100);

    emit valuesChanged(values);
}

void PerformanceWorker::start()
{
    if (m_timer)
        return;

    // Create and start timer in this method (instead of constructor) because this method is going
    // to be called from the worker thread.
    m_timer = std::make_unique<QTimer>();
    connect(m_timer.get(), &QTimer::timeout, this, &PerformanceWorker::gatherValues);
    m_timer->start(kUpdateInterval);
}

} // namespace nx::vms::client::desktop
