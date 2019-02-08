#pragma once

#include <QtCore/QThread>

#include <nx/utils/timer_manager.h>
#include <core/resource/resource_fwd.h>
#include <network/router.h>
#include <nx/network/abstract_socket.h>
#include <nx/network/time/mean_time_fetcher.h>
#include <common/common_module_aware.h>
#include <nx/vms/time_sync/time_sync_manager.h>
#include <nx/vms/network/abstract_server_connector.h>
#include <nx/utils/elapsed_timer.h>
#include <nx_ec/ec_api_fwd.h>

namespace nx {
namespace vms {
namespace time_sync {

class ServerTimeSyncManager: public TimeSyncManager
{
    Q_OBJECT;
    using base_type = TimeSyncManager;
public:

    ServerTimeSyncManager(
        QnCommonModule* commonModule,
        nx::vms::network::AbstractServerConnector* serverConnector);
    virtual ~ServerTimeSyncManager();

    virtual void stop() override;
    virtual void start() override;
    void init(const ec2::AbstractECConnectionPtr& connection);

    void setTimeFetcher(std::unique_ptr<AbstractAccurateTimeFetcher> timeFetcher);
protected:
    virtual void updateTime() override;
    virtual AbstractStreamSocketPtr connectToRemoteHost(
        const QnRoute& route, bool sslRequired) override;
private:
    bool loadTimeFromInternet();

    QnUuid getPrimaryTimeServerId() const;
    void initializeTimeFetcher();
    void broadcastSystemTime();
    QnRoute routeToNearestServerWithInternet();
    void saveSystemTimeDeltaMs(qint64 systemTimeDeltaMs);
    void updateSyncTimeToOsTimeDelta();
private:
    std::unique_ptr<AbstractAccurateTimeFetcher> m_internetTimeSynchronizer;
    std::atomic<bool> m_internetSyncInProgress {false};
    std::atomic<bool> m_updateTimePlaned {false};
    std::atomic<bool> m_broadcastTimePlaned {false};
    nx::vms::network::AbstractServerConnector* m_serverConnector = nullptr;
    nx::utils::ElapsedTimer m_lastNetworkSyncTime;
    QnUuid m_timeLoadFromServer;
    ec2::AbstractECConnectionPtr m_connection;
    qint64 m_systemTimeDeltaMs = 0;
};

} // namespace time_sync
} // namespace vms
} // namespace nx
