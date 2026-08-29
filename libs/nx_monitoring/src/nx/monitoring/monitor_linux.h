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

    virtual double totalCpuUsage() override;
    virtual std::uint64_t totalRamUsageBytes() override;
    virtual std::uint64_t thisProcessRamUsageBytes() override;
    virtual std::uint64_t thisProcessPrivateRamUsageBytes() override;
    virtual double thisProcessCpuUsage() override;
    virtual std::vector<HddLoad> totalHddLoad() override;
    virtual std::vector<NetworkLoad> totalNetworkLoad() override;
    virtual std::vector<PartitionSpace> totalPartitionSpaceInfo() override;

    virtual void setPartitionInformationProvider(
        std::unique_ptr<PartitionsInformationProvider> partitionInformationProvider) override;

    virtual int thisProcessThreads() override;

private:
    class Private;
    const std::unique_ptr<Private> d;
};

} // namespace nx::monitoring
