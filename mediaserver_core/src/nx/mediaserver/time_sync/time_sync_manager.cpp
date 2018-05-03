#include "time_sync_manager.h"

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <api/global_settings.h>
#include <nx/network/deprecated/simple_http_client.h>
#include <nx/utils/elapsed_timer.h>
#include <nx_ec/data/api_reverse_connection_data.h>
#include <media_server/media_server_module.h>
#include <nx/mediaserver/reverse_connection_manager.h>
#include <nx/network/time/time_protocol_client.h>
#include <utils/common/rfc868_servers.h>

namespace nx {
namespace mediaserver {
namespace time_sync {

static const std::chrono::seconds kProxySocetTimeout(10);

TimeSynchManager::TimeSynchManager(
    QnMediaServerModule* serverModule,
    nx::utils::StandaloneTimerManager* const /*timerManager*/,
    const std::shared_ptr<AbstractSystemClock>& systemClock,
    const std::shared_ptr<AbstractSteadyClock>& steadyClock)
    :
    ServerModuleAware(serverModule),
    m_systemClock(systemClock),
    m_steadyClock(steadyClock)
{
    initializeTimeFetcher();
}

TimeSynchManager::~TimeSynchManager()
{
    pleaseStop();
}

void TimeSynchManager::pleaseStop()
{
    if (m_internetTimeSynchronizer)
    {
        m_internetTimeSynchronizer->pleaseStopSync();
        m_internetTimeSynchronizer.reset();
    }
}

void TimeSynchManager::initializeTimeFetcher()
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

QnMediaServerResourcePtr TimeSynchManager::getPrimaryTimeServer()
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

std::unique_ptr<nx::network::AbstractStreamSocket> TimeSynchManager::connectToRemoteHost(const QnRoute& route)
{
    if (route.reverseConnect) 
    {
        const auto& target = route.gatewayId.isNull() ? route.id : route.gatewayId;
        auto manager = serverModule()->reverseConnectionManager();
        return manager->getProxySocket(target, kProxySocetTimeout);
    }
    else
    {
        auto socket = nx::network::SocketFactory::createStreamSocket(false);
        if (socket->connect(
            route.addr,
            nx::network::deprecated::kDefaultConnectTimeout))
        {
            return std::unique_ptr<nx::network::AbstractStreamSocket>();
        }
    }
    return std::unique_ptr<nx::network::AbstractStreamSocket>();
}

void TimeSynchManager::loadTimeFromServer(const QnRoute& route)
{
    auto socket = connectToRemoteHost(route);
    if (!socket)
        return;

    // Use deprecated CLSimpleHTTPClient to reduce latency. 
    // New http clients (either sync and async) are sharing IO thread pool.
    CLSimpleHTTPClient httpClient(std::move(socket));
    QUrl url;
    url.setPath("/api/getSyncTime");

    nx::utils::ElapsedTimer timer;
    timer.restart();
    auto status = httpClient.doGET(url.toString());
    if (status != nx::network::http::StatusCode::ok)
        return;
    QByteArray response;
    httpClient.readAll(response);
    if (response.isEmpty())
        return;

    auto newTime = std::chrono::milliseconds(response.toLongLong() - timer.elapsedMs() / 2);
    setTime(newTime);
    NX_DEBUG(this, lm("Received time %1 from the neighbor server %2")
        .args(QDateTime::fromMSecsSinceEpoch(newTime.count()).toString(Qt::ISODate),
        route.id.toString()));
}

void TimeSynchManager::loadTimeFromInternet()
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
            setTime(std::chrono::milliseconds(newValue));
            NX_DEBUG(this, lm("Received time %1 from the internet").
                arg(QDateTime::fromMSecsSinceEpoch(newValue).toString(Qt::ISODate)));
            m_internetSyncInProgress = false;
        });
}

void TimeSynchManager::loadTimeFromLocalClock()
{
    auto newValue = m_systemClock->millisSinceEpoch();
    setTime(newValue);
    NX_DEBUG(this, lm("Set time %1 from the local clock").
        arg(QDateTime::fromMSecsSinceEpoch(newValue.count()).toString(Qt::ISODate)));
}

void TimeSynchManager::setTime(std::chrono::milliseconds value)
{
    const auto minDeltaToSync = 
        commonModule()->globalSettings()->maxDifferenceBetweenSynchronizedAndInternetTime();
    const auto timeDelta = value - getTime();
    if (timeDelta < minDeltaToSync)
        return;

    QnMutexLocker lock(&m_mutex);
    m_synchronizedTime = value;
    m_synchronizedOnClock = m_steadyClock->now();
}

std::chrono::milliseconds TimeSynchManager::getTime() const
{
    QnMutexLocker lock(&m_mutex);
    auto elapsed = m_steadyClock->now() - m_synchronizedOnClock;
    return m_synchronizedTime + elapsed;
}

void TimeSynchManager::doPeriodicTasks()
{
    bool syncWithInternel = commonModule()->globalSettings()->isSynchronizingTimeWithInternet();

    auto server = getPrimaryTimeServer();
    if (!server)
        return;

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
        if (!route.isValid())
            loadTimeFromServer(route);
    }
}

} // namespace time_sync
} // namespace vms
} // namespace nx
