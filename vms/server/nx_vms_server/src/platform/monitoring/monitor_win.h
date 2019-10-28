#pragma once

#include "sigar_monitor.h"

class QnWindowsMonitorPrivate;

class QnWindowsMonitor: public QnSigarMonitor
{
    typedef QnSigarMonitor base_type;
public:
    QnWindowsMonitor();
    virtual ~QnWindowsMonitor();

    /** Implementation of nx::vms::server::PlatformMonitor::totalPartitionSpaceInfo */
    virtual QList<PartitionSpace> totalPartitionSpaceInfo() override;
    /** Implementation of nx::vms::server::PlatformMonitor::totalHddLoad */
    virtual QList<HddLoad> totalHddLoad() override;
    /** Implementation of nx::vms::server::PlatformMonitor::totalNetworkLoad */
    virtual QList<NetworkLoad> totalNetworkLoad() override;
    /** Implementation of nx::vms::server::PlatformMonitor::thisProcessThreads */
    virtual int thisProcessThreads() override;

private:
    Q_DECLARE_PRIVATE(QnWindowsMonitor);
    QScopedPointer<QnWindowsMonitorPrivate> d_ptr;
};
