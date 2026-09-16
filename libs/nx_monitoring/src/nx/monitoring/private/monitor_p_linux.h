// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstdint>
#include <map>
#include <unordered_map>
#include <utility>
#include <vector>

#include <sys/times.h>
#include <nx/utils/elapsed_timer.h>

#include "../monitor_linux.h"

namespace nx::monitoring {

class InterfaceStatisticsContext: public ActivityMonitor::NetworkLoad
{
public:
    std::uint64_t bytesReceived;
    std::uint64_t bytesSent;

    InterfaceStatisticsContext();

    static InterfaceStatisticsContext create(const QString& name);
    void update(int64_t elapsed);
};

class LinuxMonitor::Private
{
public:
    typedef ActivityMonitor::Hdd Hdd;
    typedef ActivityMonitor::HddLoad HddLoad;

    //!Timeout (in seconds) during which partition list expires (needed to detect mounts/umounts)
    static const time_t kPartitionListExpireTimeoutSec = 60;

    int64_t prevCPUTimeTotal;
    int64_t prevCPUTimeIdle;

public:
    Private();

    double thisProcessCpuUsage();
    std::vector<HddLoad> totalHddLoad();
    std::vector<ActivityMonitor::DiskIo> totalDiskIo();
    std::vector<ActivityMonitor::NetworkLoad> totalNetworkLoad();
    void updatePartitions();

    int calculateId(int majorNumber, int minorNumber);

    std::vector<HddLoad> zeroLoad() const;

protected:
    void calcNetworkStat();

private:
    std::unordered_map<int, Hdd> m_diskById;
    std::unordered_map<int, unsigned int> m_lastDiskTimeById;
    std::unordered_map<std::uint64_t, std::pair<std::uint64_t, std::uint64_t>>
        m_lastDiskOperationsById;
    std::map<QString, InterfaceStatisticsContext> m_ifNameToStatistics;
    nx::utils::ElapsedTimer m_networkStatCalcTimer;
    nx::utils::ElapsedTimer m_hddStatCalcTimer;
    nx::utils::ElapsedTimer m_diskIoStatCalcTimer;

    time_t m_lastPartitionsUpdateTime;
    struct timespec m_lastDiskUsageUpdateTime;

    clock_t m_previousProcessElapsedClocks;
    struct tms m_previousProcessTimes;

public:
    std::unique_ptr<ActivityMonitor::PartitionsInformationProvider> partitionsInfoProvider;
};


} // namespace nx::monitoring
