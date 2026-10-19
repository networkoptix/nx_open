// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "performance_monitor.h"

#include <chrono>

#include <QtCore/QThread>
#include <QtCore/QTimer>

#include <nx/monitoring/activity_monitor.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/desktop/ini.h>

#include "performance_worker.h"

namespace nx::vms::client::desktop {

const QString PerformanceMonitor::kProcessCpu = "CPU";
const QString PerformanceMonitor::kTotalCpu = "Total CPU";
const QString PerformanceMonitor::kGpu = "GPU";
const QString PerformanceMonitor::kMemory = "Memory";
const QString PerformanceMonitor::kThreads = "Threads";

struct PerformanceMonitor::Private
{
    QScopedPointer<QThread> workerThread;
    bool visible = false;
    bool debugInfoVisible = false;

    std::unique_ptr<monitoring::ActivityMonitor> memoryUsageMonitor;
    QTimer memoryUsageLogTimer;
};

PerformanceMonitor::PerformanceMonitor(QObject* parent):
    base_type(parent),
    d(new Private())
{
    const std::chrono::seconds memoryUsageLogPeriod(ini().memoryUsageLogPeriodSec);
    if (memoryUsageLogPeriod <= std::chrono::seconds::zero())
        return;

    d->memoryUsageMonitor = monitoring::ActivityMonitor::createForCurrentPlatform();

    // Reading the memory usage is cheap, so it is done in the main thread.
    connect(&d->memoryUsageLogTimer,
        &QTimer::timeout,
        this,
        [this]()
        {
            static constexpr std::uint64_t kBytesInMB = 1024 * 1024;

            NX_INFO(this,
                "Memory usage: %1 MB, private: %2 MB, threads: %3",
                d->memoryUsageMonitor->thisProcessRamUsageBytes() / kBytesInMB,
                d->memoryUsageMonitor->thisProcessPrivateRamUsageBytes() / kBytesInMB,
                d->memoryUsageMonitor->thisProcessThreads());
        });
    d->memoryUsageLogTimer.start(memoryUsageLogPeriod);
}

PerformanceMonitor::~PerformanceMonitor()
{
    // Stop and wait for worker thread to finish.
    setEnabled(false);
}

bool PerformanceMonitor::isVisible() const
{
    return d->visible;
}

void PerformanceMonitor::setDebugInfoVisible(bool visible)
{
    d->debugInfoVisible = visible;
}

bool PerformanceMonitor::isDebugInfoVisible() const
{
    return d->debugInfoVisible;
}

void PerformanceMonitor::setVisible(bool visible)
{
    if (isEnabled() != visible)
        setEnabled(visible);

    if (d->visible == visible)
        return;

    d->visible = visible;
    emit visibleChanged(visible);
}

bool PerformanceMonitor::isEnabled() const
{
    return d->workerThread && d->workerThread->isRunning();
}

void PerformanceMonitor::setEnabled(bool enabled)
{
    if (isEnabled() == enabled)
        return;

    if (enabled)
    {
        d->workerThread.reset(new QThread());
        auto worker = new PerformanceWorker();
        worker->moveToThread(d->workerThread.data());

        connect(d->workerThread.data(), &QThread::started,
            worker, &PerformanceWorker::start, Qt::QueuedConnection);
        connect(d->workerThread.data(), &QThread::finished,
            worker, &QObject::deleteLater);

        connect(worker, &PerformanceWorker::valuesChanged,
            this, &PerformanceMonitor::valuesChanged);

        d->workerThread->start();
    }
    else if (d->workerThread)
    {
        d->workerThread->quit();
        d->workerThread->wait();
        d->workerThread.reset();
    }
}

} // namespace nx::vms::client::desktop
