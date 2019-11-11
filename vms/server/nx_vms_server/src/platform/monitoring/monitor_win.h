#pragma once

#include "sigar_monitor.h"

class QnWindowsMonitorPrivate;

class QnWindowsMonitor: public QnSigarMonitor
{
    typedef QnSigarMonitor base_type;
public:
    QnWindowsMonitor();
    virtual ~QnWindowsMonitor();

    virtual QList<PartitionSpace> totalPartitionSpaceInfo() override;
    virtual QList<HddLoad> totalHddLoad() override;
    virtual QList<NetworkLoad> totalNetworkLoad() override;
    virtual int thisProcessThreads() override;
    virtual quint64 thisProcessRamUsageBytes() override;

private:
    Q_DECLARE_PRIVATE(QnWindowsMonitor);
    QScopedPointer<QnWindowsMonitorPrivate> d_ptr;
};
