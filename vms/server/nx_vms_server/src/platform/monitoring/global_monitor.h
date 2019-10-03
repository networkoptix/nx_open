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

    void logStatistics();

    /**
     * \returns                         Update period of this global monitor object, in milliseconds.
     */
    qint64 updatePeriodMs() const;

    /**
     * Server uptime in milliseconds.
     */
    std::chrono::milliseconds processUptime() const;

    virtual qreal totalCpuUsage() override;
    virtual quint64 totalRamUsageBytes() override;
    virtual qreal thisProcessCpuUsage() override;
    virtual quint64 thisProcessRamUsageBytes() override;
    virtual QList<HddLoad> totalHddLoad() override;
    virtual QList<NetworkLoad> totalNetworkLoad() override;
    virtual QList<PartitionSpace> totalPartitionSpaceInfo() override;
    virtual QString partitionByPath(const QString &path) override;

    virtual void setServerModule(QnMediaServerModule* serverModule) override;

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

} // namespace nx::vms::server
