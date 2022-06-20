// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "performance_worker.h"

#include "performance_monitor.h"

#include <thread>

#include <QtCore/QVariantMap>
#include <QtCore/QJsonObject>
#include <QtCore/QTimer>

#include <nx/utils/trace/trace.h>
#include <nx/monitoring/activity_monitor.h>

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

    const qreal processCpuValue = m_monitor->thisProcessCpuUsage()
        * std::thread::hardware_concurrency();
    const qreal totalCpuValue = m_monitor->totalCpuUsage()
        * std::thread::hardware_concurrency();
    const quint64 processMemoryValue = m_monitor->thisProcessRamUsageBytes();
    const quint64 processPrivateMemoryValue = m_monitor->thisProcessPrivateRamUsageBytes();
    const auto threadCount = m_monitor->thisProcessThreads();
    const auto processGpuValue = m_monitor->thisProcessGpuUsage();

    QVariantMap values;
    values[PerformanceMonitor::kCpu] = processCpuValue;
    values[PerformanceMonitor::kMemory] = processMemoryValue;
    values[PerformanceMonitor::kThreads] = threadCount;
    values[PerformanceMonitor::kGpu] = processGpuValue;

    #if defined(Q_OS_WIN)
        const auto processHandle = GetCurrentProcess();
        const quint32 gdiHandleCount = GetGuiResources(processHandle, GR_GDIOBJECTS);
        const quint32 userHandleCount = GetGuiResources(processHandle, GR_USEROBJECTS);

        values["Objects GDI"] = gdiHandleCount;
        values["Objects USER"] = userHandleCount;

        NX_TRACE_COUNTER("Gui Resources").args({
            { "GDI", (qint64) gdiHandleCount },
            { "USER", (qint64) userHandleCount }});
    #endif

    static constexpr qreal kBytesInMB = 1024.0 * 1024.0;

    // Counters are stacked, so put each counter under its own name.
    NX_TRACE_COUNTER("CPU").args({
        { "Process (%) ", processCpuValue * 100 },
        { "Total (%)", totalCpuValue * 100 }});
    NX_TRACE_COUNTER("Memory").args({
        { "Process private (MB)", (qreal) processPrivateMemoryValue / kBytesInMB },
        { "Process (MB)", (qreal) processMemoryValue / kBytesInMB },
        { "Total in use (MB)", (qreal) m_monitor->totalRamUsageBytes() / kBytesInMB }});
    NX_TRACE_COUNTER("Threads").args({{ "Threads", threadCount }});
    NX_TRACE_COUNTER("GPU").args({{ "GPU", processGpuValue * 100 }});

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
