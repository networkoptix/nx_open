#ifndef QN_LINUX_MONITOR_H
#define QN_LINUX_MONITOR_H

#include <QFuture>
#include <QtConcurrent>
#include <boost/optional.hpp>
#include "sigar_monitor.h"
#include <utils/common/systemerror.h>
#include <utils/thread/mutex.h>

class QnLinuxMonitorPrivate;

class QnLinuxMonitor: public QnSigarMonitor {
    Q_OBJECT
    typedef QnSigarMonitor base_type;

    struct PartitionsFutureInfo
    {
        bool started;
        QnMutex mutex;
        QnMutex syncMutex;
        QFuture<void> done;
        QList<PartitionSpace> info;
        PartitionsFutureInfo() : started(false) {}
    };

public:
    QnLinuxMonitor(QObject *parent = NULL);
    virtual ~QnLinuxMonitor();

    virtual qreal totalCpuUsage() override;
    virtual qreal totalRamUsage() override;
    virtual QList<HddLoad> totalHddLoad() override;
    virtual QList<QnPlatformMonitor::NetworkLoad> totalNetworkLoad() override;
    virtual QList<PartitionSpace> totalPartitionSpaceInfo() override;

private:
    Q_DECLARE_PRIVATE(QnLinuxMonitor);
    QScopedPointer<QnLinuxMonitorPrivate> d_ptr;
    PartitionsFutureInfo m_partitionsInfo;
};

#endif // QN_LINUX_MONITOR_H
