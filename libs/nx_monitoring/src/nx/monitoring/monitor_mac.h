// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>

#include "activity_monitor.h"

namespace nx::monitoring {

class NX_MONITORING_API MacMonitor: public ActivityMonitor
{
    typedef ActivityMonitor base_type;

public:
    MacMonitor();
    virtual ~MacMonitor();

    virtual std::vector<PartitionSpace> totalPartitionSpaceInfo() override;
    virtual int thisProcessThreads() override;
    virtual std::vector<NetworkLoad> totalNetworkLoad() override;
    virtual std::uint64_t totalRamUsageBytes() override;
    virtual std::uint64_t thisProcessRamUsageBytes() override;
    virtual std::uint64_t thisProcessPrivateRamUsageBytes() override;
    virtual double totalCpuUsage() override;
    virtual double thisProcessCpuUsage() override;
    virtual double thisProcessGpuUsage() override;
    virtual std::vector<HddLoad> totalHddLoad() override;

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
