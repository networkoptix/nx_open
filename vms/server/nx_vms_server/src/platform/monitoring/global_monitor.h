#pragma once

#include "platform_monitor.h"

#include <nx/utils/elapsed_timer.h>
#include <nx/utils/value_cache.h>

/**
 * A proxy monitor object suitable for usage as an application-wide monitor.
 *
 * Updates on regular time intervals and is thread-safe.
 */
class QnGlobalMonitor: public QnPlatformMonitor
{
    Q_OBJECT;
    // TODO: Can remove Q_OBJECT here?

public:
    static const std::chrono::milliseconds kCacheExpirationTime;

    /**
     * \param base                      Base platform monitor to get actual data from.
     *                                  Global monitor claims ownership of this object.
     * \param parent                    Parent of this object.
     * \param updatePeriodMs            statistics update period. It's disabled if 0.
     */
    QnGlobalMonitor(QnPlatformMonitor *base, QObject *parent);
    virtual ~QnGlobalMonitor();

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
    virtual qreal totalRamUsage() override;
    virtual QList<HddLoad> totalHddLoad() override;
    virtual QList<NetworkLoad> totalNetworkLoad() override;
    virtual QList<PartitionSpace> totalPartitionSpaceInfo() override;
    virtual QString partitionByPath(const QString &path) override;

    virtual void setServerModule(QnMediaServerModule* serverModule) override;

private:
    QnPlatformMonitor* m_monitorBase;
    mutable QnMutex m_mutex;
    nx::utils::ElapsedTimer m_uptimeTimer;

    nx::utils::CachedValue<qreal> m_cachedTotalCpuUsage;
    nx::utils::CachedValue<qreal> m_cachedTotalRamUsage;
    nx::utils::CachedValue<QList<QnPlatformMonitor::HddLoad>> m_cachedTotalHddLoad;
    nx::utils::CachedValue<QList<QnPlatformMonitor::NetworkLoad>> m_cachedTotalNetworkLoad;


};
