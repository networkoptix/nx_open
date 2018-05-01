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

void TimeSynchManager::updateTimeFromInternet()
{

}

std::unique_ptr<nx::network::AbstractStreamSocket> TimeSynchManager::connectToRemoteHost(const QnRoute& route)
{
    std::unique_ptr<nx::network::AbstractStreamSocket> socket;
    if (route.reverseConnect) 
    {
        const auto& target = route.gatewayId.isNull() ? route.id : route.gatewayId;
        auto manager = serverModule()->reverseConnectionManager();
        socket = manager->getProxySocket(target, kProxySocetTimeout);
    }
    else
    {
        socket = nx::network::SocketFactory::createStreamSocket(false);
        if (socket->connect(
            route.addr,
            nx::network::deprecated::kDefaultConnectTimeout))
        {
            return std::unique_ptr<nx::network::AbstractStreamSocket>();
        }
    }
    return socket;
}

void TimeSynchManager::loadTimeFromServer(const QnRoute& route)
{
    auto socket = connectToRemoteHost(route);
    if (!socket)
        return;
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

    m_synchronizedTimeMs = response.toLongLong() - timer.elapsedMs()/2;
}

void TimeSynchManager::doPeriodicTasks()
{
    bool syncWithInternel = commonModule()->globalSettings()->isSynchronizingTimeWithInternet();

    auto server = getPrimaryTimeServer();
    if (server && server->getId() == commonModule()->moduleGUID())
    {
        if (syncWithInternel)
            updateTimeFromInternet();
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
