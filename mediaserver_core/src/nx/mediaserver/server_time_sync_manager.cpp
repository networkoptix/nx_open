#include "server_time_sync_manager.h"

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <api/global_settings.h>
#include <nx/utils/elapsed_timer.h>
#include <nx_ec/data/api_reverse_connection_data.h>
#include <media_server/media_server_module.h>
#include <nx/mediaserver/reverse_connection_manager.h>
#include <nx/network/time/time_protocol_client.h>
#include <utils/common/rfc868_servers.h>
#include <nx/network/socket_factory.h>
#include <nx/network/http/http_client.h>
#include <nx/utils/time.h>
#include <utils/common/delayed.h>
#include <transaction/transaction.h>
#include <transaction/message_bus_adapter.h>

namespace nx {
namespace mediaserver {

static const std::chrono::seconds kProxySocetTimeout(10);
static const std::chrono::seconds kMinTimeUpdateInterval(10);
static const QByteArray kTimeDeltaParamName = "sync_time_delta";


bool compareTimePriority(
    const QnMediaServerResourcePtr& left, const QnMediaServerResourcePtr& right)
{
    bool leftIsOnline = left->getStatus();
    bool rightIsOnline = right->getStatus();
    if (leftIsOnline != rightIsOnline)
        return leftIsOnline < rightIsOnline;

    bool leftHasInternet = left->getServerFlags().testFlag(Qn::SF_HasPublicIP);
    bool rightHasInternet = right->getServerFlags().testFlag(Qn::SF_HasPublicIP);
    if (leftHasInternet != rightHasInternet)
        return leftHasInternet < rightHasInternet;

    return left->getId() < right->getId();
};


ServerTimeSyncManager::ServerTimeSyncManager(
    QnCommonModule* commonModule,
    ReverseConnectionManager* reverseConnectionManager,
    const std::shared_ptr<AbstractSystemClock>& systemClock,
    const std::shared_ptr<AbstractSteadyClock>& steadyClock)
    :
    base_type(commonModule, systemClock, steadyClock),
    m_reverseConnectionManager(reverseConnectionManager)
{

    connect(
        commonModule->resourcePool(), &QnResourcePool::statusChanged,
        this, [this](const QnResourcePtr& resource, Qn::StatusChangeReason /*reason*/)
    {
        if (auto server = resource.dynamicCast<QnMediaServerResource>())
        {
            auto commonModule = this->commonModule();
            QnMediaServerResourcePtr ownServer = commonModule->resourcePool()
                ->getResourceById<QnMediaServerResource>(commonModule->moduleGUID());
            if (!ownServer)
                return;

            bool isLess = compareTimePriority(server, ownServer);
            if (!isLess)
                return;

            if (m_updateTimePlaned.exchange(true))
                return;

            executeDelayed([this]()
            {
                m_updateTimePlaned = false;
                updateTime();
            },
            std::chrono::milliseconds(kMinTimeUpdateInterval).count(),
            this->thread());
        }
    });
    connect(this, &ServerTimeSyncManager::timeChanged, this,
        [this]()
        {
            auto primaryTimeServerId = getPrimaryTimeServerId();
            if (primaryTimeServerId == this->commonModule()->moduleGUID())
                broadcastSystemTimeDelayed();
        });
}

void ServerTimeSyncManager::broadcastSystemTimeDelayed()
{
    if (m_broadcastTimePlaned.exchange(true))
        return;

    executeDelayed([this]()
    {
        m_broadcastTimePlaned = false;
        broadcastSystemTime();
    },
        std::chrono::milliseconds(kMinTimeUpdateInterval).count(),
        this->thread());
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
    stop();
}

void ServerTimeSyncManager::start()
{
    ec2::ApiMiscData deltaData;
    auto connection = commonModule()->ec2Connection();
    auto miscManager = connection->getMiscManager(Qn::kSystemAccess);
    auto dbResult = miscManager->getMiscParamSync(kTimeDeltaParamName, &deltaData);
    if (dbResult  != ec2::ErrorCode::ok)
    {
        NX_WARNING(this, "Failed to load time delta parameter from the database");
    }
    auto timeDelta = std::chrono::milliseconds(deltaData.value.toLongLong());
    setSyncTimeInternal(m_systemClock->millisSinceEpoch() + timeDelta);
    
    initializeTimeFetcher();
    base_type::start();
}

void ServerTimeSyncManager::setSyncTime(std::chrono::milliseconds value)
{
    base_type::setSyncTime(value);

    const auto systemTimeDeltaMs = value.count() - m_systemClock->millisSinceEpoch().count();
    auto connection = commonModule()->ec2Connection();
    ec2::ApiMiscData deltaData(
        kTimeDeltaParamName,
        QByteArray::number(systemTimeDeltaMs));

    connection->getMiscManager(Qn::kSystemAccess)->saveMiscParam(
        deltaData, this,
        [this](int /*reqID*/, ec2::ErrorCode errCode)
    {
        if (errCode != ec2::ErrorCode::ok)
            NX_WARNING(this, lm("Failed to save time delta data to the database"));
    });

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
    auto resourcePool = commonModule()->resourcePool();
    auto settings = commonModule()->globalSettings();
    QnUuid primaryTimeServer = settings->primaryTimeServer();
    if (!primaryTimeServer.isNull())
    {
        // User-defined time server.
        auto server = resourcePool->getResourceById<QnMediaServerResource>(primaryTimeServer);
        return server ? server->getId() : QnUuid();
    }
    
    // Automatically select primary time server.
    auto servers = resourcePool->getAllServers(Qn::Online);
    if (servers.isEmpty())
        return QnUuid();
    std::sort(servers.begin(), servers.end(), compareTimePriority);
    return servers.front()->getId();
}

std::unique_ptr<nx::network::AbstractStreamSocket> ServerTimeSyncManager::connectToRemoteHost(const QnRoute& route)
{
    if (route.reverseConnect && m_reverseConnectionManager)
    {
        const auto& target = route.gatewayId.isNull() ? route.id : route.gatewayId;
        return m_reverseConnectionManager->getProxySocket(target, kProxySocetTimeout);
    }
    else
    {
        return base_type::connectToRemoteHost(route);
    }
}

void ServerTimeSyncManager::loadTimeFromInternet()
{
    if (m_internetSyncInProgress.exchange(true))
        return; //< Sync already in progress.

    m_internetTimeSynchronizer->getTimeAsync(
        [this](const qint64 newValue, SystemError::ErrorCode errorCode)
        {
            if (errorCode)
            {
                NX_WARNING(this, lm("Failed to get time from the internet. %1")
                    .arg(SystemError::toString(errorCode)));
                return;
            }
            setSyncTime(std::chrono::milliseconds(newValue));
            NX_DEBUG(this, lm("Received time %1 from the internet").
                arg(QDateTime::fromMSecsSinceEpoch(newValue).toString(Qt::ISODate)));
            m_internetSyncInProgress = false;
        });
}

void ServerTimeSyncManager::updateTime()
{
    auto primaryTimeServerId = getPrimaryTimeServerId();
    if (primaryTimeServerId.isNull())
        return;

    bool syncWithInternel = commonModule()->globalSettings()->isSynchronizingTimeWithInternet();
    if (primaryTimeServerId == commonModule()->moduleGUID())
    {
        if (syncWithInternel)
            loadTimeFromInternet();
        else
            loadTimeFromLocalClock();
    }
    else
    {
        auto route = commonModule()->router()->routeTo(primaryTimeServerId);
        if (route.isValid())
            loadTimeFromServer(route);
    }
}

} // namespace mediaserver
} // namespace nx
