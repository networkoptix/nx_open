#ifndef QN_SIGAR_MONITOR_H
#define QN_SIGAR_MONITOR_H

#include "platform_monitor.h"

#include <QtCore/QScopedPointer>

class QnSigarMonitorPrivate;

class QnSigarMonitor: public QnPlatformMonitor {
    Q_OBJECT
public:
    QnSigarMonitor(QObject *parent = NULL);
    virtual ~QnSigarMonitor();

    virtual qreal totalCpuUsage() override;

    virtual qreal totalRamUsage() override;

    virtual QList<HddLoad> totalHddLoad() override;

    virtual QList<PartitionSpace> totalPartitionSpaceInfo() override;

    virtual QList<NetworkLoad> totalNetworkLoad() override;

private:
    Q_DECLARE_PRIVATE(QnSigarMonitor)
    QScopedPointer<QnSigarMonitorPrivate> d_ptr;
};


#endif // QN_SIGAR_MONITOR_H
