// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "performance_monitor.h"

#include "performance_worker.h"

#include <QtCore/QThread>

#include <nx/utils/trace/trace.h>

namespace nx::vms::client::desktop {

const QString PerformanceMonitor::kCpu = "CPU";
const QString PerformanceMonitor::kGpu = "GPU";
const QString PerformanceMonitor::kMemory = "Memory";
const QString PerformanceMonitor::kThreads = "Threads";

struct PerformanceMonitor::Private
{
    QScopedPointer<QThread> workerThread;
    bool visible = false;
    bool debugInfoVisible = false;
};

PerformanceMonitor::PerformanceMonitor(QObject* parent):
    base_type(parent),
    d(new Private())
{
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
    if (d->visible == visible)
        return;

    setEnabled(nx::utils::trace::Log::isEnabled() || visible);

    d->visible = visible;
    emit visibleChanged(visible);
}

void PerformanceMonitor::setEnabled(bool enabled)
{
    const bool isRunning = d->workerThread && d->workerThread->isRunning();
    if (isRunning == enabled)
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
