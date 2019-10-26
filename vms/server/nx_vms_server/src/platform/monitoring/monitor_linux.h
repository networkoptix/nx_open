#pragma once

#include "sigar_monitor.h"

class QnLinuxMonitorPrivate;
namespace nx::vms::server { class RootFileSystem; }

class QnLinuxMonitor: public QnSigarMonitor
{
    typedef QnSigarMonitor base_type;
public:
    QnLinuxMonitor();
    virtual ~QnLinuxMonitor();

    virtual qreal totalCpuUsage() override;
    virtual quint64 totalRamUsageBytes() override;
    virtual quint64 thisProcessRamUsageBytes() override;
    virtual QList<HddLoad> totalHddLoad() override;
    virtual QList<nx::vms::server::PlatformMonitor::NetworkLoad> totalNetworkLoad() override;
    virtual QList<PartitionSpace> totalPartitionSpaceInfo() override;

    virtual void setServerModule(QnMediaServerModule* serverModule) override;

    virtual int thisProcessThreads() override;

private:
    Q_DECLARE_PRIVATE(QnLinuxMonitor);
    QScopedPointer<QnLinuxMonitorPrivate> d_ptr;
};
