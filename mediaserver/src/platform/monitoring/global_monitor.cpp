#include "global_monitor.h"

#include <QtCore/QBasicTimer>
#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>
#include <utils/common/mutex.h>
#include <utils/common/mutex.h>

#include <utils/common/warnings.h>
#include <utils/common/delete_later.h>
#include <utils/common/log.h>


// -------------------------------------------------------------------------- //
// QnStubMonitor
// -------------------------------------------------------------------------- //
class QnStubMonitor: public QnPlatformMonitor {
public:
    QnStubMonitor(QObject *parent = NULL): QnPlatformMonitor(parent) {}

    virtual qreal totalCpuUsage() override { return 0.0; }
    virtual qreal totalRamUsage() override { return 0.0; }
    virtual QList<HddLoad> totalHddLoad() override { return QList<HddLoad>(); }
    virtual QList<NetworkLoad> totalNetworkLoad() override { return QList<NetworkLoad>(); }
    virtual QList<PartitionSpace> totalPartitionSpaceInfo() override { return QList<PartitionSpace>(); }
    virtual QString partitionByPath(const QString &) override { return QString(); }
};


// -------------------------------------------------------------------------- //
// QnGlobalMonitorPrivate
// -------------------------------------------------------------------------- //
class QnGlobalMonitorPrivate {
public:
    QnGlobalMonitorPrivate()
    :
        base(NULL),
        updatePeriod(2500),
        stopped(true),
        requestCount(0),
        totalCpuUsage(0.0),
        totalRamUsage(0.0),
        prevCpuUsageLoggingClock(0),
        prevMemUsageLoggingClock(0),
        prevHddUsageLoggingClock(0),
        prevNetworkUsageLoggingClock(0)
    {
        upTimeTimer.start();
    }

    virtual ~QnGlobalMonitorPrivate() {}

    void restartTimersLocked() {
        updateTimer.start(updatePeriod, q_func());
        stopTimer.start(updatePeriod * 64, q_func());
    }

    void updateCacheLocked() {
        totalCpuUsage = base->totalCpuUsage();
        totalRamUsage = base->totalRamUsage();
        totalHddLoad = base->totalHddLoad();
        totalNetworkLoad = base->totalNetworkLoad();
    }

private:
    mutable QnMutex mutex;

    QnPlatformMonitor *base;
    qint64 updatePeriod;
    bool stopped;
    int requestCount;

    qreal totalCpuUsage;
    qreal totalRamUsage;
    QList<QnPlatformMonitor::HddLoad> totalHddLoad;
    QList<QnPlatformMonitor::NetworkLoad> totalNetworkLoad;

    QBasicTimer updateTimer;
    QBasicTimer stopTimer;
    QElapsedTimer upTimeTimer;

    qint64 prevCpuUsageLoggingClock;
    qint64 prevMemUsageLoggingClock;
    qint64 prevHddUsageLoggingClock;
    qint64 prevNetworkUsageLoggingClock;

private:
    Q_DECLARE_PUBLIC(QnGlobalMonitor)
    QnGlobalMonitor *q_ptr;
};


// -------------------------------------------------------------------------- //
// QnGlobalMonitor
// -------------------------------------------------------------------------- //
QnGlobalMonitor::QnGlobalMonitor(QnPlatformMonitor *base, QObject *parent):
    base_type(parent),
    d_ptr(new QnGlobalMonitorPrivate())
{
    Q_D(QnGlobalMonitor);

    d_ptr->q_ptr = this;

    if(!base) {
        qnNullWarning(base);
        base = new QnStubMonitor();
    }

    if(base->thread() != thread()) {
        qnWarning("Cannot use a base monitor that lives in another thread.");
        qnDeleteLater(base); /* Claim ownership. */
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
    SCOPED_MUTEX_LOCK( locker, &d->mutex);

    return d->updatePeriod;
}

void QnGlobalMonitor::setUpdatePeriod(qint64 updatePeriod) {
    Q_D(QnGlobalMonitor);
    SCOPED_MUTEX_LOCK( locker, &d->mutex);

    if(d->updatePeriod == updatePeriod)
        return;

    d->updatePeriod = updatePeriod;
    d->restartTimersLocked();
}

static const int STATISTICS_LOGGING_PERIOD_MS = 10 * 60 * 1000;

qreal QnGlobalMonitor::totalCpuUsage() {
    Q_D(QnGlobalMonitor);
    SCOPED_MUTEX_LOCK( locker, &d->mutex);

    d->requestCount++;
    d->stopped = false;

    //printing usage to logs, but not frequently than STATISTICS_LOGGING_PERIOD_MS
    if( d->upTimeTimer.elapsed() - d->prevCpuUsageLoggingClock > STATISTICS_LOGGING_PERIOD_MS )
    {
        NX_LOG(lit("MONITORING. Cpu usage %1%").arg(d->totalCpuUsage*100, 0, 'f', 2), cl_logALWAYS);
        d->prevCpuUsageLoggingClock = d->upTimeTimer.elapsed();
    }

    return d->totalCpuUsage;
}

qreal QnGlobalMonitor::totalRamUsage() {
    Q_D(QnGlobalMonitor);
    SCOPED_MUTEX_LOCK( locker, &d->mutex);

    d->requestCount++;
    d->stopped = false;

    //printing usage to logs, but not frequently than STATISTICS_LOGGING_PERIOD_MS
    if( d->upTimeTimer.elapsed() - d->prevMemUsageLoggingClock > STATISTICS_LOGGING_PERIOD_MS )
    {
        NX_LOG(lit("MONITORING. Memory usage %1%").arg(d->totalRamUsage * 100, 0, 'f', 2), cl_logALWAYS);
        d->prevMemUsageLoggingClock = d->upTimeTimer.elapsed();
    }

    return d->totalRamUsage;
}

QList<QnPlatformMonitor::HddLoad> QnGlobalMonitor::totalHddLoad() {
    Q_D(QnGlobalMonitor);
    SCOPED_MUTEX_LOCK( locker, &d->mutex);

    d->requestCount++;
    d->stopped = false;

    //printing usage to logs, but not frequently than STATISTICS_LOGGING_PERIOD_MS
    if( d->upTimeTimer.elapsed() - d->prevHddUsageLoggingClock > STATISTICS_LOGGING_PERIOD_MS )
    {
        NX_LOG(lit("MONITORING. HDD usage:"), cl_logALWAYS);
        for( const HddLoad& hddLoad : d->totalHddLoad )
            NX_LOG(lit("    %1: %2%").arg(hddLoad.hdd.name).arg(hddLoad.load * 100, 0, 'f', 2), cl_logALWAYS);
        d->prevHddUsageLoggingClock = d->upTimeTimer.elapsed();
    }

    return d->totalHddLoad;
}

QList<QnPlatformMonitor::NetworkLoad> QnGlobalMonitor::totalNetworkLoad() {
    Q_D(QnGlobalMonitor);
    SCOPED_MUTEX_LOCK( locker, &d->mutex);

    d->requestCount++;
    d->stopped = false;

    //printing usage to logs, but not frequently than STATISTICS_LOGGING_PERIOD_MS
    if( d->upTimeTimer.elapsed() - d->prevNetworkUsageLoggingClock > STATISTICS_LOGGING_PERIOD_MS )
    {
        NX_LOG(lit("MONITORING. Network usage:"), cl_logALWAYS);
        for( const NetworkLoad& networkLoad : d->totalNetworkLoad )
            NX_LOG(lit("    %1. in %2KBps, out %3KBps").arg(networkLoad.interfaceName).arg(networkLoad.bytesPerSecIn / 1024).arg(networkLoad.bytesPerSecOut / 1024), cl_logALWAYS);
        d->prevNetworkUsageLoggingClock = d->upTimeTimer.elapsed();
    }

    return d->totalNetworkLoad;
}

QList<QnPlatformMonitor::PartitionSpace> QnGlobalMonitor::totalPartitionSpaceInfo() {
    Q_D(QnGlobalMonitor);
    SCOPED_MUTEX_LOCK( locker, &d->mutex);

    return d_func()->base->totalPartitionSpaceInfo();
}

QString QnGlobalMonitor::partitionByPath(const QString &path) {
    Q_D(QnGlobalMonitor);
    SCOPED_MUTEX_LOCK( locker, &d->mutex);

    return d_func()->base->partitionByPath(path);
}

void QnGlobalMonitor::timerEvent(QTimerEvent *event) {
    Q_D(QnGlobalMonitor);

    SCOPED_MUTEX_LOCK( locker, &d->mutex);

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

qint64 QnGlobalMonitor::upTimeMs() const
{
    Q_D(const QnGlobalMonitor);
    return d->upTimeTimer.elapsed();
}
