// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../monitor_linux.h"
#include <nx/utils/elapsed_timer.h>
#include <sys/times.h>

namespace nx::monitoring {

class InterfaceStatisticsContext: public ActivityMonitor::NetworkLoad
{
public:
    uint64_t bytesReceived;
    uint64_t bytesSent;

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

    qreal thisProcessCpuUsage();
    QList<HddLoad> totalHddLoad();
    QList<ActivityMonitor::NetworkLoad> totalNetworkLoad();
    void updatePartitions();

    int calculateId(int majorNumber, int minorNumber);

    QList<HddLoad> zeroLoad() const;

protected:
    void calcNetworkStat();

private:
    QHash<int, Hdd> m_diskById;
    QHash<int, unsigned int> m_lastDiskTimeById;
    std::map<QString, InterfaceStatisticsContext> m_ifNameToStatistics;
    nx::utils::ElapsedTimer m_networkStatCalcTimer;
    nx::utils::ElapsedTimer m_hddStatCalcTimer;

    time_t m_lastPartitionsUpdateTime;
    struct timespec m_lastDiskUsageUpdateTime;

    clock_t m_previousProcessElapsedClocks;
    struct tms m_previousProcessTimes;

public:
    std::unique_ptr<ActivityMonitor::PartitionsInformationProvider> partitionsInfoProvider;
};


} // namespace nx::monitoring
