// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>

#include "activity_monitor.h"

namespace nx::monitoring {

class WindowsMonitorPrivate;

class NX_MONITORING_API WindowsMonitor: public ActivityMonitor
{
    typedef ActivityMonitor base_type;

public:
    WindowsMonitor();
    virtual ~WindowsMonitor();

    virtual double totalCpuUsage() override;
    virtual std::uint64_t totalRamUsageBytes() override;
    virtual double thisProcessCpuUsage() override;
    virtual double thisProcessGpuUsage() override;
    virtual std::vector<PartitionSpace> totalPartitionSpaceInfo() override;
    virtual std::vector<HddLoad> totalHddLoad() override;
    virtual std::vector<NetworkLoad> totalNetworkLoad() override;
    virtual int thisProcessThreads() override;
    virtual std::uint64_t thisProcessRamUsageBytes() override;
    virtual std::uint64_t thisProcessPrivateRamUsageBytes() override;

private:
    Q_DECLARE_PRIVATE(WindowsMonitor);
    QScopedPointer<WindowsMonitorPrivate> d_ptr;
};

} // namespace nx::monitoring
