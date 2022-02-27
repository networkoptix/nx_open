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

    virtual qreal totalCpuUsage() override;
    virtual quint64 totalRamUsageBytes() override;
    virtual qreal thisProcessCpuUsage() override;
    virtual qreal thisProcessGpuUsage() override;
    virtual QList<PartitionSpace> totalPartitionSpaceInfo() override;
    virtual QList<HddLoad> totalHddLoad() override;
    virtual QList<NetworkLoad> totalNetworkLoad() override;
    virtual int thisProcessThreads() override;
    virtual quint64 thisProcessRamUsageBytes() override;
    virtual quint64 thisProcessPrivateRamUsageBytes() override;

private:
    Q_DECLARE_PRIVATE(WindowsMonitor);
    QScopedPointer<WindowsMonitorPrivate> d_ptr;
};

} // namespace nx::monitoring
