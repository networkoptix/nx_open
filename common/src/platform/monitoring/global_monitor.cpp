#include "global_monitor.h"

#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QBasicTimer>
#include <QtCore/QCoreApplication>

#include <utils/common/warnings.h>

// -------------------------------------------------------------------------- //
// QnStubMonitor
// -------------------------------------------------------------------------- //
class QnStubMonitor: public QnPlatformMonitor {
public:
    QnStubMonitor(QObject *parent = NULL): QnPlatformMonitor(parent) {}

    virtual qreal totalCpuUsage() override { return 0.0; }
    virtual qreal totalRamUsage() override { return 0.0; }
    virtual QList<HddLoad> totalHddLoad() override { return QList<HddLoad>(); }
    virtual QList<PartitionSpace> totalPartitionSpaceInfo() override { return QList<PartitionSpace>(); }
    virtual QString partitionByPath(const QString &path) override { return QString(); }
};


// -------------------------------------------------------------------------- //
// QnGlobalMonitorPrivate
// -------------------------------------------------------------------------- //
class QnGlobalMonitorPrivate {
public:
    QnGlobalMonitorPrivate():
        base(NULL),
        updatePeriod(2500),
        stopped(true),
        requestCount(0),
        totalCpuUsage(0.0),
        totalRamUsage(0.0)
    {}

    virtual ~QnGlobalMonitorPrivate() {}

    void restartTimersLocked() {
        updateTimer.start(updatePeriod, q_func());
        stopTimer.start(updatePeriod * 64, q_func());
    }

    void updateCacheLocked() {
        totalCpuUsage = base->totalCpuUsage();
        totalRamUsage = base->totalRamUsage();
        totalHddLoad = base->totalHddLoad();
    }

private:
    mutable QMutex mutex;

    QnPlatformMonitor *base;
    qint64 updatePeriod;
    bool stopped;
    int requestCount;

    qreal totalCpuUsage;
    qreal totalRamUsage;
    QList<QnPlatformMonitor::HddLoad> totalHddLoad;

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
    Q_UNUSED(parent)
    Q_D(QnGlobalMonitor);

    d_ptr->q_ptr = this;

    if(!base) {
        qnNullWarning(base);
        base = new QnStubMonitor();
    }

    if(base->thread() != thread()) {
        qnWarning("Cannot use a base monitor that lives in another thread.");
        base = new QnStubMonitor();
    }

    base->setParent(this); /* Claim ownership. */
    d->base = base;

    d->updateCacheLocked();
    d->restartTimersLocked();
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
    d->restartTimersLocked();
}

qreal QnGlobalMonitor::totalCpuUsage() {
    Q_D(QnGlobalMonitor);

    /* Note: there is no need for locks here, at least on x86. */

    d->requestCount++;
    d->stopped = false;
    return d->totalCpuUsage;
}

qreal QnGlobalMonitor::totalRamUsage() {
    Q_D(QnGlobalMonitor);

    /* Note: there is no need for locks here, at least on x86. */

    d->requestCount++;
    d->stopped = false;
    return d->totalRamUsage;
}

QList<QnPlatformMonitor::HddLoad> QnGlobalMonitor::totalHddLoad() {
    Q_D(QnGlobalMonitor);
    QMutexLocker locker(&d->mutex);

    d->requestCount++;
    d->stopped = false;
    return d->totalHddLoad;
}

QList<QnPlatformMonitor::PartitionSpace> QnGlobalMonitor::totalPartitionSpaceInfo() {
    return d_func()->base->totalPartitionSpaceInfo();
}

void QnGlobalMonitor::timerEvent(QTimerEvent *event) {
    Q_D(QnGlobalMonitor);

    QMutexLocker locker(&d->mutex);

    if(event->timerId() == d->updateTimer.timerId()) {
        if(!d->stopped)
            d->updateCacheLocked();
    } else if(event->timerId() == d->stopTimer.timerId()) {
        if(d->requestCount == 0) {
            d->stopped = true;
        } else {
            d->requestCount = 0;
        }
    } else {
        locker.unlock();
        base_type::timerEvent(event);
    }
}
