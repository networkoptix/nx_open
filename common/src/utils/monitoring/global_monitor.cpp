#include "global_monitor.h"

#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QBasicTimer>
#include <QtCore/QCoreApplication>

#include <utils/common/warnings.h>

// -------------------------------------------------------------------------- //
// QnGlobalMonitorPrivate
// -------------------------------------------------------------------------- //
class QnGlobalMonitorPrivate {
public:
    QnGlobalMonitorPrivate():
        base(NULL),
        updatePeriod(1000),
        stopped(true),
        totalCpuUsage(0.0),
        totalRamUsage(0.0),
        lastUpdateTime(0)
    {}

    virtual ~QnGlobalMonitorPrivate() {}

private:
    QnPlatformMonitor *base;
    qint64 updatePeriod;
    bool stopped;
    int requestCount;
    mutable QMutex mutex;

    qreal totalCpuUsage;
    qreal totalRamUsage;
    QHash<QnPlatformMonitor::Hdd, qreal> totalUsageByHdd;

    qint64 lastUpdateTime;

    QBasicTimer updateTimer;
    QBasicTimer stopTimer;

private:
    Q_DECLARE_PUBLIC(QnGlobalMonitor);
    QnGlobalMonitor *q_ptr;
};


// -------------------------------------------------------------------------- //
// QnGlobalMonitor
// -------------------------------------------------------------------------- //
QnGlobalMonitor::QnGlobalMonitor(QnPlatformMonitor *base, QObject *parent):
    d_ptr(new QnGlobalMonitorPrivate())
{
    Q_D(QnGlobalMonitor);

    d->q_ptr = this;

    if(!base) {
        qnNullWarning(base);
        base = new QnPlatformMonitor();
    }

    if(base->thread() != thread()) {
        qnWarning("Cannot use a base monitor that lives in another thread.");
        base = new QnPlatformMonitor();
    }

    base->setParent(this); /* Claim ownership. */
    d->base = base;
}

QnGlobalMonitor::~QnGlobalMonitor() {
    return;
}

qint64 QnGlobalMonitor::updatePeriod() const {
    Q_D(const QnGlobalMonitor);
    QMutexLocker locker(&d->mutex);

    return d->updatePeriod;
}

void QnGlobalMonitor::setUpdatePeriod(qint64 updatePeriod) {
    Q_D(QnGlobalMonitor);
    QMutexLocker locker(&d->mutex);

    if(d->updatePeriod == updatePeriod)
        return;

    d->updatePeriod = updatePeriod;
    d->updateTimer.start(updatePeriod, this);
    d->stopTimer.start(updatePeriod * 100, this);
}

qreal QnGlobalMonitor::totalCpuUsage() {
    Q_D(QnGlobalMonitor);

    /* Note: there is no need for locks here. */

    d->requestCount++;
    d->stopped = false;
    return d->totalCpuUsage;
}

qreal QnGlobalMonitor::totalRamUsage() {
    Q_D(QnGlobalMonitor);

    /* Note: there is no need for locks here. */

    d->requestCount++;
    d->stopped = false;
    return d->totalRamUsage;
}

QList<QnPlatformMonitor::Hdd> QnGlobalMonitor::hdds() {
    Q_D(QnGlobalMonitor);
    QMutexLocker locker(&d->mutex);

}

qreal QnGlobalMonitor::totalHddLoad(const Hdd &hdd) {

}

