#pragma once

#include "platform_monitor.h"

#include <nx/utils/elapsed_timer.h>
#include <nx/utils/value_cache.h>

namespace nx::vms::server {

qreal ramUsageToPercentages(quint64 bytes);

/**
 * A proxy monitor object suitable for usage as an application-wide monitor.
 *
 * Updates on regular time intervals and is thread-safe.
 */
class GlobalMonitor: public nx::vms::server::PlatformMonitor
{
    Q_OBJECT;

public:
    static const std::chrono::milliseconds kCacheExpirationTime;

    /**
     * \param base                      Base platform monitor to get actual data from.
     *                                  Global monitor claims ownership of this object.
     * \param parent                    Parent of this object.
     * \param updatePeriodMs            statistics update period. It's disabled if 0.
     */
    GlobalMonitor(nx::vms::server::PlatformMonitor *base, QObject *parent);
    virtual ~GlobalMonitor();

    virtual void logStatistics() override;
    virtual qreal totalCpuUsage() override;
    virtual quint64 totalRamUsageBytes() override;
    virtual qreal thisProcessCpuUsage() override;
    virtual quint64 thisProcessRamUsageBytes() override;
    virtual QList<HddLoad> totalHddLoad() override;
    virtual QList<NetworkLoad> totalNetworkLoad() override;
    virtual QList<PartitionSpace> totalPartitionSpaceInfo() override;
    virtual QString partitionByPath(const QString &path) override;
    virtual std::chrono::milliseconds processUptime() const override;
    virtual std::chrono::milliseconds updatePeriod() const override;
    virtual void setServerModule(QnMediaServerModule* serverModule) override;
    virtual int thisProcessThreads() override;

private:
    nx::vms::server::PlatformMonitor* m_monitorBase = nullptr;
    mutable QnMutex m_mutex;
    nx::utils::ElapsedTimer m_uptimeTimer;

    nx::utils::CachedValue<qreal> m_cachedTotalCpuUsage;
    nx::utils::CachedValue<quint64> m_cachedTotalRamUsage;
    nx::utils::CachedValue<qreal> m_cachedThisProcessCpuUsage;
    nx::utils::CachedValue<quint64> m_cachedThisProcessRamUsage;
    nx::utils::CachedValue<QList<nx::vms::server::PlatformMonitor::HddLoad>> m_cachedTotalHddLoad;
    nx::utils::CachedValue<QList<nx::vms::server::PlatformMonitor::NetworkLoad>> m_cachedTotalNetworkLoad;
};

class StubMonitor: public nx::vms::server::PlatformMonitor
{
public:
    StubMonitor(QObject *parent = NULL): nx::vms::server::PlatformMonitor(parent) {}

    virtual qreal totalCpuUsage() override { return totalCpuUsage_; }
    virtual quint64 totalRamUsageBytes() override { return totalRamUsageBytes_; }
    virtual qreal thisProcessCpuUsage() override { return thisProcessCpuUsage_; }
    virtual quint64 thisProcessRamUsageBytes() override { return thisProcessRamUsageBytes_; }
    virtual QList<HddLoad> totalHddLoad() override { return {}; }
    virtual QList<NetworkLoad> totalNetworkLoad() override { return {}; }
    virtual QList<PartitionSpace> totalPartitionSpaceInfo() override { return {}; }
    virtual QString partitionByPath(const QString &) override { return {}; }
    virtual std::chrono::milliseconds processUptime() const override { return processUptime_; }
    virtual int thisProcessThreads() override { return 1; }

public:
    std::atomic<qreal> totalCpuUsage_{0};
    std::atomic<quint64> totalRamUsageBytes_{0};
    std::atomic<qreal> thisProcessCpuUsage_{0};
    std::atomic<quint64> thisProcessRamUsageBytes_{0};
    std::atomic<std::chrono::milliseconds> processUptime_{std::chrono::seconds(1)};
};

} // namespace nx::vms::server
