// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "activity_monitor.h"

#include <QtCore/QScopedPointer>

namespace nx::monitoring {

class NX_MONITORING_API MacMonitor: public ActivityMonitor
{
    typedef ActivityMonitor base_type;

public:
    MacMonitor();
    virtual ~MacMonitor();

    virtual QList<PartitionSpace> totalPartitionSpaceInfo() override;
    virtual int thisProcessThreads() override;
    virtual QList<NetworkLoad> totalNetworkLoad() override;
    virtual quint64 totalRamUsageBytes() override;
    virtual quint64 thisProcessRamUsageBytes() override;
    virtual quint64 thisProcessPrivateRamUsageBytes() override;
    virtual qreal totalCpuUsage() override;
    virtual qreal thisProcessCpuUsage() override;
    virtual qreal thisProcessGpuUsage() override;
    virtual QList<HddLoad> totalHddLoad() override;

private:
    class NetworkLoadMonitor;
    class CpuLoadMonitor;
    class GpuLoadMonitor;
    class HddLoadMonitor;

    QScopedPointer<NetworkLoadMonitor> m_networkLoadMonitor;
    QScopedPointer<CpuLoadMonitor> m_cpuLoadMonitor;
    QScopedPointer<GpuLoadMonitor> m_gpuLoadMonitor;
    QScopedPointer<HddLoadMonitor> m_hddLoadMonitor;
};

} // namespace nx::monitoring
