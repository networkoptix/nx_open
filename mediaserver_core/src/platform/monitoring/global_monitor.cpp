
#include "global_monitor.h"

#ifdef __linux__
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/resource.h>
#endif

#include <iostream>
#include <QtCore/QBasicTimer>
#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/mutex.h>

#include <utils/common/warnings.h>
#include <utils/common/delete_later.h>
#include <nx/utils/log/log.h>

// Uncomment to enable malloc statistics debug output
//#define MALLOC_STATISTICS

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
        updatePeriod(0),
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
QnGlobalMonitor::QnGlobalMonitor(
    QnPlatformMonitor *base,
    QObject *parent)
:
    base_type(parent),
    d_ptr(new QnGlobalMonitorPrivate())
{
    Q_D(QnGlobalMonitor);

    d_ptr->q_ptr = this;

    if(!base)
    {
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
    if (d->updatePeriod > 0)
    {
        d->updateCacheLocked();
        d->restartTimersLocked();
    }
}

QnGlobalMonitor::~QnGlobalMonitor() {
    return;
}

qint64 QnGlobalMonitor::updatePeriodMs() const
{
    Q_D(const QnGlobalMonitor);
    QnMutexLocker locker( &d->mutex );

    return d->updatePeriod;
}

void QnGlobalMonitor::setUpdatePeriodMs(qint64 updatePeriod)
{
    Q_D(QnGlobalMonitor);
    QnMutexLocker locker( &d->mutex );

    if(d->updatePeriod == updatePeriod)
        return;

    d->updatePeriod = updatePeriod;
    d->restartTimersLocked();
}

static const int STATISTICS_LOGGING_PERIOD_MS = 30 * 60 * 1000;

qreal QnGlobalMonitor::totalCpuUsage() {
    Q_D(QnGlobalMonitor);
    QnMutexLocker locker( &d->mutex );

    d->requestCount++;
    d->stopped = false;

    //printing usage to logs, but not frequently than STATISTICS_LOGGING_PERIOD_MS
    if( d->upTimeTimer.elapsed() - d->prevCpuUsageLoggingClock > STATISTICS_LOGGING_PERIOD_MS )
    {
        NX_LOG(lit("MONITORING. Cpu usage %1%").arg(d->totalCpuUsage*100, 0, 'f', 2), cl_logWARNING);
        d->prevCpuUsageLoggingClock = d->upTimeTimer.elapsed();
    }

    return d->totalCpuUsage;
}

qreal QnGlobalMonitor::totalRamUsage() {
    Q_D(QnGlobalMonitor);
    QnMutexLocker locker( &d->mutex );

    d->requestCount++;
    d->stopped = false;

    //printing usage to logs, but not frequently than STATISTICS_LOGGING_PERIOD_MS
    if( d->upTimeTimer.elapsed() - d->prevMemUsageLoggingClock > STATISTICS_LOGGING_PERIOD_MS )
    {
        NX_LOG(lit("MONITORING. Memory usage %1%").arg(d->totalRamUsage * 100, 0, 'f', 2), cl_logWARNING);
        d->prevMemUsageLoggingClock = d->upTimeTimer.elapsed();

#ifdef __linux__
        std::cerr << std::endl << "-----------------------------> malloc_stats info start " << std::endl;
        malloc_stats();
        std::cerr << "-----------------------------> malloc_stats info end" << std::endl << std::endl;
#endif
    }

    return d->totalRamUsage;
}

#if defined (Q_OS_LINUX)
void logOpenedHandleCount()
{
	int fdCount = 0;
	char buf[64];
	struct dirent *dp;

	snprintf(buf, 64, "/proc/%i/fd/", getpid());

	DIR *dir = opendir(buf);
	while ((dp = readdir(dir)) != NULL)
		fdCount++;
	closedir(dir);
	NX_LOG(lit("    Opened: %1").arg(fdCount), cl_logWARNING);
}
#elif defined (Q_OS_WIN)
void logOpenedHandleCount()
{
	DWORD typeChar = 0;
	DWORD typeDisk = 0;
	DWORD typePipe = 0;
	DWORD typeRemote = 0;
	DWORD typeUnknown = 0;
	DWORD handlesCount = 0;

	GetProcessHandleCount(GetCurrentProcess(), &handlesCount);
	handlesCount *= 4;
	for (DWORD handle = 0x4; handle < handlesCount; handle += 4)
	{
		switch (GetFileType((HANDLE)handle))
		{
			case FILE_TYPE_CHAR:
				typeChar++;
				break;
			case FILE_TYPE_DISK:
				typeDisk++;
				break;
			case FILE_TYPE_PIPE:
				typePipe++;
				break;
			case FILE_TYPE_REMOTE:
				typeRemote++;
				break;
			case FILE_TYPE_UNKNOWN:
				if (GetLastError() == NO_ERROR) typeUnknown++;
				break;
		}
	}

	NX_LOG(lit("    Disk files: %1").arg(typeDisk), cl_logWARNING);
	NX_LOG(lit("    Sockets, pipes: %1").arg(typePipe), cl_logWARNING);
	NX_LOG(lit("    Character devices: %1").arg(typeChar), cl_logWARNING);
	NX_LOG(lit("    Unknown: %1").arg(typeUnknown), cl_logWARNING);
}
#else
void logOpenedHandleCount()
{
	NX_LOG(lit("    Not implemented for this platform"), cl_logWARNING);
}
#endif

QList<QnPlatformMonitor::HddLoad> QnGlobalMonitor::totalHddLoad() {
    Q_D(QnGlobalMonitor);
    QnMutexLocker locker( &d->mutex );

    d->requestCount++;
    d->stopped = false;

    //printing usage to logs, but not frequently than STATISTICS_LOGGING_PERIOD_MS
    if( d->upTimeTimer.elapsed() - d->prevHddUsageLoggingClock > STATISTICS_LOGGING_PERIOD_MS )
    {
        NX_LOG(lit("MONITORING. HDD usage:"), cl_logWARNING);
        for( const HddLoad& hddLoad : d->totalHddLoad )
            NX_LOG(lit("    %1: %2%").arg(hddLoad.hdd.name).arg(hddLoad.load * 100, 0, 'f', 2), cl_logWARNING);
        NX_LOG(lit("MONITORING. File handles:"), cl_logWARNING);
		logOpenedHandleCount();
        d->prevHddUsageLoggingClock = d->upTimeTimer.elapsed();

        #if defined(__linux__) && defined(MALLOC_STATISTICS)
            const size_t memStatBufSize = 64*1024;
            std::vector<char> memStatBuf;
            memStatBuf.resize(memStatBufSize);
            FILE* memStatStr = fmemopen(memStatBuf.data(), memStatBufSize, "w");
            malloc_info(0, memStatStr);
            fclose(memStatStr);
            NX_LOG(lit("MONITORING. malloc statistics: \n%1").arg(memStatBuf.data()), cl_logWARNING);
        #endif
    }

    return d->totalHddLoad;
}

QList<QnPlatformMonitor::NetworkLoad> QnGlobalMonitor::totalNetworkLoad() {
    Q_D(QnGlobalMonitor);
    QnMutexLocker locker( &d->mutex );

    d->requestCount++;
    d->stopped = false;

    //printing usage to logs, but not frequently than STATISTICS_LOGGING_PERIOD_MS
    if( d->upTimeTimer.elapsed() - d->prevNetworkUsageLoggingClock > STATISTICS_LOGGING_PERIOD_MS )
    {
        NX_LOG(lit("MONITORING. Network usage:"), cl_logWARNING);
        for( const NetworkLoad& networkLoad : d->totalNetworkLoad )
            NX_LOG(lit("    %1. in %2KBps, out %3KBps").arg(networkLoad.interfaceName).
                arg(networkLoad.bytesPerSecIn / 1024).arg(networkLoad.bytesPerSecOut / 1024), cl_logWARNING);
        d->prevNetworkUsageLoggingClock = d->upTimeTimer.elapsed();
    }

    return d->totalNetworkLoad;
}

QList<QnPlatformMonitor::PartitionSpace> QnGlobalMonitor::totalPartitionSpaceInfo() {
    Q_D(QnGlobalMonitor);
    QnMutexLocker locker( &d->mutex );

    return d_func()->base->totalPartitionSpaceInfo();
}

QString QnGlobalMonitor::partitionByPath(const QString &path) {
    Q_D(QnGlobalMonitor);
    QnMutexLocker locker( &d->mutex );

    return d_func()->base->partitionByPath(path);
}

void QnGlobalMonitor::timerEvent(QTimerEvent *event) {
    Q_D(QnGlobalMonitor);

    QnMutexLocker locker( &d->mutex );

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
