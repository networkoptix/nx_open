#pragma once

#include <QtCore/QThread>

#include <nx/utils/timer_manager.h>
#include <core/resource/resource_fwd.h>
#include <network/router.h>
#include <nx/network/abstract_socket.h>
#include <nx/network/time/mean_time_fetcher.h>
#include <common/common_module_aware.h>
#include <nx/time_sync/time_sync_manager.h>

namespace nx {
namespace mediaserver {

class ReverseConnectionManager;

class ServerTimeSyncManager: public nx::time_sync::TimeSyncManager
{
    Q_OBJECT;
    using base_type = nx::time_sync::TimeSyncManager;
public:

    /**
     * TimeSynchronizationManager::start MUST be called before using class instance.
     */
    ServerTimeSyncManager(
        QnCommonModule* commonModule,
        ReverseConnectionManager* reverseConnectionManager,
        const std::shared_ptr<AbstractSystemClock>& systemClock = nullptr,
        const std::shared_ptr<AbstractSteadyClock>& steadyClock = nullptr);
    virtual ~ServerTimeSyncManager();

    virtual void stop() override;
    virtual void start() override;

protected:
    virtual void updateTime() override;
    virtual AbstractStreamSocketPtr connectToRemoteHost(const QnRoute& route) override;
    virtual void setSyncTime(std::chrono::milliseconds value) override;
private:
    void loadTimeFromInternet();

    QnUuid getPrimaryTimeServerId() const;
    void initializeTimeFetcher();
    void broadcastSystemTime();
    void broadcastSystemTimeDelayed();
private:
    std::unique_ptr<AbstractAccurateTimeFetcher> m_internetTimeSynchronizer;
    std::atomic<bool> m_internetSyncInProgress{false};
    std::atomic<bool> m_updateTimePlaned{ false };
    std::atomic<bool> m_broadcastTimePlaned{ false };
    ReverseConnectionManager* m_reverseConnectionManager = nullptr;
};

} // namespace mediaserver
} // namespace nx
