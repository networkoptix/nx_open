#include "server_time_sync_manager.h"

#include <api/global_settings.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <transaction/message_bus_adapter.h>
#include <transaction/transaction.h>
#include <utils/common/rfc868_servers.h>
#include <utils/common/delayed.h>

#include <nx/network/http/http_client.h>
#include <nx/network/socket_factory.h>
#include <nx/network/time/time_protocol_client.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/utils/time.h>
#include <nx/vms/api/data/misc_data.h>

namespace nx {
namespace vms {
namespace time_sync {

static const QByteArray kTimeDeltaParamName = "sync_time_delta"; //< For migration from previous version.

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
            saveSystemTimeDeltaMs(systemTimeDeltaMs);

            auto primaryTimeServerId = getPrimaryTimeServerId();
            if (primaryTimeServerId == this->commonModule()->moduleGUID())
            {
                NX_VERBOSE(this,
                    lm("Peer %1 broadcast time has changed to %2")
                    .arg(qnStaticCommon->moduleDisplayName(this->commonModule()->moduleGUID()))
                    .arg(QDateTime::fromMSecsSinceEpoch(syncTimeMs).toString(Qt::ISODate)));
                broadcastSystemTime();
            }
        });
}

void ServerTimeSyncManager::broadcastSystemTime()
{
    using namespace ec2;
    auto connection = commonModule()->ec2Connection();
    QnTransaction<nx::vms::api::PeerSyncTimeData> tran(
        ApiCommand::broadcastPeerSyncTime,
        commonModule()->moduleGUID());
    tran.params.syncTimeMs = getSyncTime().count();
    if (auto connection = commonModule()->ec2Connection())
        connection->messageBus()->sendTransaction(tran);
}

ServerTimeSyncManager::~ServerTimeSyncManager()
{
    stop();
}

void ServerTimeSyncManager::start()
{
    initializeTimeFetcher();
    m_connection = commonModule()->ec2Connection();
    connect(
        m_connection->getTimeNotificationManager().get(),
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

    if (m_connection)
        disconnect(m_connection->getTimeNotificationManager().get());
}

void ServerTimeSyncManager::initializeTimeFetcher()
{
    if (m_internetTimeSynchronizer)
        return;

    auto meanTimerFetcher = std::make_unique<nx::network::MeanTimeFetcher>();
    for (const char* timeServer: RFC868_SERVERS)
    {
        meanTimerFetcher->addTimeFetcher(
            std::make_unique<nx::network::TimeProtocolClient>(
                QLatin1String(timeServer)));
    }

    m_internetTimeSynchronizer = std::move(meanTimerFetcher);
}

void ServerTimeSyncManager::setTimeFetcher(std::unique_ptr<AbstractAccurateTimeFetcher> timeFetcher)
{
    NX_ASSERT(!m_internetTimeSynchronizer);
    auto meanTimerFetcher = std::make_unique<nx::network::MeanTimeFetcher>();
    meanTimerFetcher->addTimeFetcher(std::move(timeFetcher));
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
        if (!server->getServerFlags().testFlag(nx::vms::api::SF_HasPublicIP))
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

void ServerTimeSyncManager::saveSystemTimeDeltaMs(qint64 systemTimeDeltaMs)
{
    m_systemTimeDeltaMs = systemTimeDeltaMs;
    auto connection = this->commonModule()->ec2Connection();
    vms::api::MiscData deltaData(
        kTimeDeltaParamName,
        QByteArray::number(systemTimeDeltaMs));

    // TODO: Avoid passing `this` in async callback.
    const auto logTag = nx::utils::log::Tag(this);
    connection->getMiscManager(Qn::kSystemAccess)->saveMiscParam(
        deltaData,
        this,
        [logTag](int /*reqID*/, ec2::ErrorCode errorCode)
        {
            if (errorCode != ec2::ErrorCode::ok)
                NX_WARNING(logTag, "Failed to save time delta data to the database");
        });
}

void ServerTimeSyncManager::updateSyncTimeToOsTimeDelta()
{
    const qint64 systemTimeDeltaMs = qAbs(
        std::chrono::milliseconds(getSyncTime() - m_systemClock->millisSinceEpoch()).count());
    if (qAbs(m_systemTimeDeltaMs - systemTimeDeltaMs) > kMaxJitterForLocalClock.count())
        saveSystemTimeDeltaMs(systemTimeDeltaMs);
}

void ServerTimeSyncManager::updateTime()
{
    updateSyncTimeToOsTimeDelta();

    const auto primaryTimeServerId = getPrimaryTimeServerId();
    const auto ownId = commonModule()->moduleGUID();
    bool syncWithInternel = primaryTimeServerId.isNull();

    auto globalSettings = commonModule()->globalSettings();
    auto networkTimeSyncInterval = globalSettings->syncTimeExchangePeriod();
    const bool isTimeSyncEnabled = globalSettings->isTimeSynchronizationEnabled();
    const bool isTimeRecentlySync = m_lastNetworkSyncTime.isValid()
        && !m_lastNetworkSyncTime.hasExpired(networkTimeSyncInterval);

    if (primaryTimeServerId == ownId || !isTimeSyncEnabled)
    {
        // Reset network time sync period. If user will change settings, next network request
        // should be applied immediately without regular delay.
        m_lastNetworkSyncTime.invalidate();

        loadTimeFromLocalClock();
    }
    else if (syncWithInternel)
    {
        QnRoute route = routeToNearestServerWithInternet();
        if (!route.isValid() && !route.reverseConnect)
            return;
        if (isTimeRecentlySync && m_timeLoadFromServer == route.id)
            return;

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

void ServerTimeSyncManager::init(const ec2::AbstractECConnectionPtr& connection)
{
    vms::api::MiscData deltaData;
    auto miscManager = connection->getMiscManager(Qn::kSystemAccess);
    auto dbResult = miscManager->getMiscParamSync(kTimeDeltaParamName, &deltaData);
    if (dbResult != ec2::ErrorCode::ok)
    {
        NX_WARNING(this, "Failed to load time delta parameter from the database");
    }
    m_systemTimeDeltaMs = deltaData.value.toLongLong();
    setSyncTimeInternal(m_systemClock->millisSinceEpoch() + std::chrono::milliseconds(m_systemTimeDeltaMs));
}

} // namespace time_sync
} // namespace vms
} // namespace nx
