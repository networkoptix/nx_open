#include "server_time_sync_manager.h"

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <api/global_settings.h>
#include <nx/utils/elapsed_timer.h>
#include <nx_ec/data/api_reverse_connection_data.h>
#include <nx/network/time/time_protocol_client.h>
#include <utils/common/rfc868_servers.h>
#include <nx/network/socket_factory.h>
#include <nx/network/http/http_client.h>
#include <nx/utils/time.h>
#include <utils/common/delayed.h>
#include <transaction/transaction.h>
#include <transaction/message_bus_adapter.h>

namespace nx {
namespace time_sync {

static const QByteArray kTimeDeltaParamName = "sync_time_delta";

ServerTimeSyncManager::ServerTimeSyncManager(
    QnCommonModule* commonModule,
    nx::vms::network::AbstractServerConnector* serverConnector)
    :
    base_type(commonModule),
    m_serverConnector(serverConnector)
{

    connect(this, &ServerTimeSyncManager::timeChanged, this,
        [this](qint64 syncTimeMs)
        {
            const auto systemTimeDeltaMs = syncTimeMs - m_systemClock->millisSinceEpoch().count();
            auto connection = this->commonModule()->ec2Connection();
            ec2::ApiMiscData deltaData(
                kTimeDeltaParamName,
                QByteArray::number(systemTimeDeltaMs));

            // Avoid passing this to async callback.
            auto logTag = nx::utils::log::Tag(toString(typeid(this)));
            connection->getMiscManager(Qn::kSystemAccess)->saveMiscParam(
                deltaData, this,
                [logTag](int /*reqID*/, ec2::ErrorCode errorCode)
            {
                if (errorCode != ec2::ErrorCode::ok)
                    NX_WARNING(logTag, lm("Failed to save time delta data to the database"));
            });

            auto primaryTimeServerId = getPrimaryTimeServerId();
            if (primaryTimeServerId == this->commonModule()->moduleGUID())
                broadcastSystemTime();
        });
}

void ServerTimeSyncManager::broadcastSystemTime()
{
    using namespace ec2;
    auto connection = commonModule()->ec2Connection();
    QnTransaction<ApiPeerSyncTimeData> tran(
        ApiCommand::broadcastPeerSyncTime,
        commonModule()->moduleGUID());
    tran.params.syncTimeMs = getSyncTime().count();
    if (auto connection = commonModule()->ec2Connection())
        connection->messageBus()->sendTransaction(tran);
}

ServerTimeSyncManager::~ServerTimeSyncManager()
{
    if (commonModule()->ec2Connection())
        disconnect(commonModule()->ec2Connection()->getTimeNotificationManager().get());
    stop();
}

void ServerTimeSyncManager::start()
{
    ec2::ApiMiscData deltaData;
    auto connection = commonModule()->ec2Connection();
    auto miscManager = connection->getMiscManager(Qn::kSystemAccess);
    auto dbResult = miscManager->getMiscParamSync(kTimeDeltaParamName, &deltaData);
    if (dbResult != ec2::ErrorCode::ok)
    {
        NX_WARNING(this, "Failed to load time delta parameter from the database");
    }
    auto timeDelta = std::chrono::milliseconds(deltaData.value.toLongLong());
    setSyncTimeInternal(m_systemClock->millisSinceEpoch() + timeDelta);

    initializeTimeFetcher();

    connect(
        commonModule()->ec2Connection()->getTimeNotificationManager().get(),
        &ec2::AbstractTimeNotificationManager::primaryTimeServerTimeChanged,
        this,
        [this]()
        {
            m_lastNetworkSyncTime.invalidate();
        });

    base_type::start();
}

void ServerTimeSyncManager::stop()
{
    base_type::stop();

    if (m_internetTimeSynchronizer)
    {
        m_internetTimeSynchronizer->pleaseStopSync();
        m_internetTimeSynchronizer.reset();
    }
}

void ServerTimeSyncManager::initializeTimeFetcher()
{
    auto meanTimerFetcher = std::make_unique<nx::network::MeanTimeFetcher>();
    for (const char* timeServer: RFC868_SERVERS)
    {
        meanTimerFetcher->addTimeFetcher(
            std::make_unique<nx::network::TimeProtocolClient>(
                QLatin1String(timeServer)));
    }

    m_internetTimeSynchronizer = std::move(meanTimerFetcher);
}

QnUuid ServerTimeSyncManager::getPrimaryTimeServerId() const
{
    auto settings = commonModule()->globalSettings();
    return settings->primaryTimeServer();
}

std::unique_ptr<nx::network::AbstractStreamSocket> ServerTimeSyncManager::connectToRemoteHost(const QnRoute& route)
{
    const auto maxRtt =
        commonModule()->globalSettings()->maxDifferenceBetweenSynchronizedAndLocalTime();

    if (m_serverConnector)
        return m_serverConnector->connectTo(route, maxRtt);
    return base_type::connectToRemoteHost(route);
}

bool ServerTimeSyncManager::loadTimeFromInternet()
{
    if (m_internetSyncInProgress.exchange(true))
        return false; //< Sync already in progress.

    m_internetTimeSynchronizer->getTimeAsync(
        [this](const qint64 newValue, SystemError::ErrorCode errorCode, std::chrono::milliseconds rtt)
        {
            if (errorCode)
            {
                NX_WARNING(this, lm("Failed to get time from the internet. %1")
                    .arg(SystemError::toString(errorCode)));
                m_lastNetworkSyncTime.invalidate();
                return;
            }
            const auto maxRtt =
                commonModule()->globalSettings()->maxDifferenceBetweenSynchronizedAndInternetTime();
            if (rtt > maxRtt)
            {
                NX_WARNING(this, lm("Failed to get time from the internet. To big rtt value %1 > %2.")
                    .args(rtt, maxRtt));
                m_lastNetworkSyncTime.invalidate();
                return;
            }

            setSyncTime(std::chrono::milliseconds(newValue), rtt);
            NX_DEBUG(this, lm("Received time %1 from the internet").
                arg(QDateTime::fromMSecsSinceEpoch(newValue).toString(Qt::ISODate)));
            m_internetSyncInProgress = false;
            m_isTimeTakenFromInternet = true;
        });
    return true;
}

QnRoute ServerTimeSyncManager::routeToNearestServerWithInternet()
{
    auto servers = commonModule()->resourcePool()->getAllServers(Qn::Online);
    int minDistance = std::numeric_limits<int>::max();
    QnRoute result;
    for (const auto& server: servers)
    {
        if (!server->getServerFlags().testFlag(Qn::SF_HasPublicIP))
            continue;
        auto route = commonModule()->router()->routeTo(server->getId());
        if (route.distance < minDistance)
        {
            result = route;
            minDistance = route.distance;
        }
    }
    return result;
}

void ServerTimeSyncManager::updateTime()
{
    const auto primaryTimeServerId = getPrimaryTimeServerId();
    const auto ownId = commonModule()->moduleGUID();
    bool syncWithInternel = commonModule()->globalSettings()->isSynchronizingTimeWithInternet();

    auto networkTimeSyncInterval = commonModule()->globalSettings()->syncTimeExchangePeriod();
    const bool isTimeRecentlySync = m_lastNetworkSyncTime.isValid()
        && !m_lastNetworkSyncTime.hasExpired(networkTimeSyncInterval);

    if (syncWithInternel)
    {
        QnRoute route = routeToNearestServerWithInternet();
        if (!route.isValid() && !route.reverseConnect)
            return;
        if (isTimeRecentlySync && m_timeLoadFromServer == route.id)
            return;
        m_timeLoadFromServer = route.id;
        m_lastNetworkSyncTime.restart();

        bool success = false;
        if (route.id == ownId)
            success = loadTimeFromInternet();
        else
            success = loadTimeFromServer(route);
        if (success)
        {
            m_timeLoadFromServer = route.id;
            m_lastNetworkSyncTime.restart();
        }
    }
    else if (primaryTimeServerId.isNull() || primaryTimeServerId == ownId)
    {
        loadTimeFromLocalClock();
    }
    else
    {
        auto route = commonModule()->router()->routeTo(primaryTimeServerId);
        if (route.isValid() || route.reverseConnect)
        {
            if (isTimeRecentlySync && m_timeLoadFromServer == route.id)
                return;
            if (loadTimeFromServer(route))
            {
                m_timeLoadFromServer = route.id;
                m_lastNetworkSyncTime.restart();
            }
        }
    }
}

bool ServerTimeSyncManager::isTimeTakenFromInternet() const
{
    return m_isTimeTakenFromInternet;
}
} // namespace time_sync
} // namespace nx
