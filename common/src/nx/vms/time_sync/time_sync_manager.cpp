#include "time_sync_manager.h"
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <network/router.h>
#include <api/global_settings.h>

namespace nx {
namespace vvms {
namespace time_sync {

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

QSharedPointer<nx::network::AbstractStreamSocket> TimeSynchManager::connectToRemoteHost(const QnRoute& route)
{
    QSharedPointer<nx::network::AbstractStreamSocket> socket;
    if (route.reverseConnect) 
    {
        const auto& target = route.gatewayId.isNull() ? route.id : route.gatewayId;
        socket = owner->getProxySocket(
            target.toString(),
            d->connectTimeout.count(),
            [&](int socketCount)
        {
            ec2::QnTransaction<ec2::ApiReverseConnectionData> tran(
                ec2::ApiCommand::openReverseConnection,
                commonModule()->moduleGUID());
            tran.params.targetServer = commonModule()->moduleGUID();
            tran.params.socketCount = socketCount;
            d->messageBus->sendTransaction(tran, target);
        });
    }
    else
    {
        socket = QSharedPointer<nx::network::AbstractStreamSocket>(
            nx::network::SocketFactory::createStreamSocket(url.scheme() == lit("https"))
            .release());
        if (!d->dstSocket->connect(
            route.addr,
            nx::network::deprecated::kDefaultConnectTimeout))
        {
            return QSharedPointer<nx::network::AbstractStreamSocket>();
        }
    }
    return socket;
}

void TimeSynchManager::loadTimeFromServer(const QnRoute& route)
{
    auto socket = connectToRemoteHost(route);
    if (!socket)
        return;
    nx::http::SyncHttpClient client;
    client.setSocket(socket);
    QUrl url;
    url.setPath("api/getSyncTime");
    
    auto data = url.doGet(url);
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
