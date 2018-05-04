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

namespace nx {
namespace mediaserver {

static const std::chrono::seconds kProxySocetTimeout(10);
const QString kTimeSyncUrlPath = QString::fromLatin1("ec2/timeSync");
static const QByteArray kTimeDeltaParamName = "sync_time_delta";


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
            updateTime();
    });

    initializeTimeFetcher();
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
    setSyncTime(m_systemClock->millisSinceEpoch() + timeDelta);

    connection->getMiscManager(Qn::kSystemAccess)->saveMiscParam(
        deltaData, this,
        [this](int /*reqID*/, ec2::ErrorCode errCode)
    {
        if (errCode != ec2::ErrorCode::ok)
            NX_WARNING(this, lm("Failed to save time delta data to the database"));
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

QnMediaServerResourcePtr ServerTimeSyncManager::getPrimaryTimeServer()
{
    auto resourcePool = commonModule()->resourcePool();
    auto settings = commonModule()->globalSettings();
    QnUuid primaryTimeServer = settings->primaryTimeServer();
    if (!primaryTimeServer.isNull())
        return resourcePool->getResourceById<QnMediaServerResource>(primaryTimeServer); //< User-defined time server.
    
    // Automatically select primary time server.
    auto servers = resourcePool->getAllServers(Qn::Online);
    if (servers.isEmpty())
        return QnMediaServerResourcePtr();
    std::sort(servers.begin(), servers.end(),
        [](const QnMediaServerResourcePtr& left, const QnMediaServerResourcePtr& right)
        {
            bool leftHasInternet = left->getServerFlags().testFlag(Qn::SF_HasPublicIP);
            bool rightHasInternet = right->getServerFlags().testFlag(Qn::SF_HasPublicIP);
            if (leftHasInternet != rightHasInternet)
                return leftHasInternet < rightHasInternet;
            return left->getId() < right->getId();
        });
    return servers.front();
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
    auto server = getPrimaryTimeServer();
    if (!server)
        return;

    bool syncWithInternel = commonModule()->globalSettings()->isSynchronizingTimeWithInternet();
    if (server->getId() == commonModule()->moduleGUID())
    {
        if (syncWithInternel)
            loadTimeFromInternet();
        else
            loadTimeFromLocalClock();
    }
    else
    {
        auto route = commonModule()->router()->routeTo(server->getId());
        if (route.isValid())
            loadTimeFromServer(route);
    }
}

} // namespace mediaserver
} // namespace nx
