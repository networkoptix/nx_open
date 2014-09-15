#ifndef QN_LINUX_MONITOR_H
#define QN_LINUX_MONITOR_H

#include "sigar_monitor.h"

class QnLinuxMonitorPrivate;

class QnLinuxMonitor: public QnSigarMonitor {
    Q_OBJECT
    typedef QnSigarMonitor base_type;

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
};

#endif // QN_LINUX_MONITOR_H
