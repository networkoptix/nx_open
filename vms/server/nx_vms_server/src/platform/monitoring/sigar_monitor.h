#ifndef QN_SIGAR_MONITOR_H
#define QN_SIGAR_MONITOR_H

#include "platform_monitor.h"

#include <QtCore/QScopedPointer>

class QnSigarMonitorPrivate;

class QnSigarMonitor: public nx::vms::server::PlatformMonitor {
    Q_OBJECT
public:
    QnSigarMonitor(QObject *parent = NULL);
    virtual ~QnSigarMonitor();

    virtual qreal totalCpuUsage() override;
    virtual quint64 totalRamUsageBytes() override;
    virtual qreal thisProcessCpuUsage() override;
    virtual quint64 thisProcessRamUsageBytes() override;
    virtual QList<HddLoad> totalHddLoad() override;
    virtual QList<PartitionSpace> totalPartitionSpaceInfo() override;
    virtual QList<NetworkLoad> totalNetworkLoad() override;

private:
    Q_DECLARE_PRIVATE(QnSigarMonitor)
    QScopedPointer<QnSigarMonitorPrivate> d_ptr;
};


#endif // QN_SIGAR_MONITOR_H
