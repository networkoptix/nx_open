#include "resource_pool_peer_manager.h"

#include <nx/utils/log/assert.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <api/server_rest_connection.h>
#include <rest/server/json_rest_result.h>
#include <common/common_module.h>

namespace nx {
namespace vms {
namespace common {
namespace distributed_file_downloader {

ResourcePoolPeerManager::ResourcePoolPeerManager(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
}

QString ResourcePoolPeerManager::peerString(const QnUuid& peerId) const
{
    const auto& server = resourcePool()->getResourceById<QnMediaServerResource>(peerId);
    if (!server)
        return lit("Unknown server %1").arg(peerId.toString());

    return lit("%1 (%2 %3)").arg(
        server->getName(),
        server->getPrimaryAddress().toString(),
        server->getId().toString());
}

QList<QnUuid> ResourcePoolPeerManager::getAllPeers() const
{
    const auto& servers = resourcePool()->getAllServers(Qn::Online);
    const auto& currentId = commonModule()->moduleGUID();

    QList<QnUuid> result;

    for (const auto& server: servers)
    {
        const auto& id = server->getId();
        if (id != currentId)
            result.append(id);
    }

    return result;
}

rest::Handle ResourcePoolPeerManager::requestFileInfo(
    const QnUuid& peerId, const QString& fileName, FileInfoCallback callback)
{
    const auto& connection = getConnection(peerId);
    if (!connection)
        return -1;

    auto handleReply =
        [this, callback](bool success, rest::Handle handle, const QnJsonRestResult& result)
        {
            if (!success)
                callback(success, handle, DownloaderFileInformation());

            const auto& fileInfo = result.deserialized<DownloaderFileInformation>();
            callback(success, handle, fileInfo);
        };

    return connection->downloaderFileStatus(fileName, handleReply, thread());
}

void ResourcePoolPeerManager::cancelRequest(const QnUuid& peerId, rest::Handle handle)
{
    const auto& connection = getConnection(peerId);
    if (!connection)
        return;

    connection->cancelRequest(handle);
}

QnMediaServerResourcePtr ResourcePoolPeerManager::getServer(const QnUuid& peerId) const
{
    return resourcePool()->getResourceById<QnMediaServerResource>(peerId);
}

rest::QnConnectionPtr ResourcePoolPeerManager::getConnection(const QnUuid& peerId) const
{
    const auto& server = getServer(peerId);
    if (!server)
        return rest::QnConnectionPtr();

    return server->restConnection();
}

ResourcePoolPeerManagerFactory::ResourcePoolPeerManagerFactory(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
}

AbstractPeerManager* ResourcePoolPeerManagerFactory::createPeerManager()
{
    return new ResourcePoolPeerManager(commonModule());
}

} // namespace distributed_file_downloader
} // namespace common
} // namespace vms
} // namespace nx
