// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>

#include "activity_monitor.h"

namespace nx::monitoring {

class RootFileSystem;

class NX_MONITORING_API LinuxMonitor: public ActivityMonitor
{
    typedef ActivityMonitor base_type;

public:
    LinuxMonitor();
    virtual ~LinuxMonitor() override;

    virtual qreal totalCpuUsage() override;
    virtual quint64 totalRamUsageBytes() override;
    virtual quint64 thisProcessRamUsageBytes() override;
    virtual quint64 thisProcessPrivateRamUsageBytes() override;
    virtual qreal thisProcessCpuUsage() override;
    virtual QList<HddLoad> totalHddLoad() override;
    virtual QList<NetworkLoad> totalNetworkLoad() override;
    virtual QList<PartitionSpace> totalPartitionSpaceInfo() override;

    virtual void setPartitionInformationProvider(
        std::unique_ptr<PartitionsInformationProvider> partitionInformationProvider) override;

    virtual int thisProcessThreads() override;

private:
    class Private;
    const std::unique_ptr<Private> d;
};

} // namespace nx::monitoring
