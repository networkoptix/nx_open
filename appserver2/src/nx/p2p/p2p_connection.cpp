#include "p2p_connection.h"

#include <common/common_module.h>
#include <api/global_settings.h>
#include <nx/network/http/custom_headers.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>

namespace nx {
namespace p2p {

Connection::Connection(
    QnCommonModule* commonModule,
    const QnUuid& remoteId,
    const ApiPeerDataEx& localPeer,
    ConnectionLockGuard connectionLockGuard,
    const QUrl& remotePeerUrl,
    std::unique_ptr<QObject> opaqueObject)
    :
    ConnectionBase(
        remoteId,
        localPeer,
        std::move(connectionLockGuard),
        remotePeerUrl,
        commonModule->globalSettings()->connectionKeepAliveTimeout(),
        std::move(opaqueObject)),
    QnCommonModuleAware(commonModule)
{
}

Connection::Connection(
    QnCommonModule* commonModule,
    const ApiPeerDataEx& remotePeer,
    const ApiPeerDataEx& localPeer,
    ConnectionLockGuard connectionLockGuard,
    nx::network::WebSocketPtr webSocket,
    const Qn::UserAccessData& userAccessData,
    std::unique_ptr<QObject> opaqueObject)
    :
    ConnectionBase(
        remotePeer,
        localPeer,
        std::move(connectionLockGuard),
        std::move(webSocket),
        userAccessData,
        std::move(opaqueObject)),
    QnCommonModuleAware(commonModule)
{
}

void Connection::fillAuthInfo(nx_http::AsyncClient* httpClient, bool authByKey)
{
    if (!commonModule()->videowallGuid().isNull())
    {
        httpClient->addAdditionalHeader(
            Qn::VIDEOWALL_GUID_HEADER_NAME,
            commonModule()->videowallGuid().toString().toUtf8());
        return;
    }

    const auto& resPool = commonModule()->resourcePool();
    QnMediaServerResourcePtr server =
        resPool->getResourceById<QnMediaServerResource>(remotePeer().id);
    if (!server)
        server = resPool->getResourceById<QnMediaServerResource>(localPeer().id);
    if (server && authByKey)
    {
        httpClient->setUserName(server->getId().toString().toLower());
        httpClient->setUserPassword(server->getAuthKey());
    }
    else
    {
        QUrl url;
        if (const auto& connection = commonModule()->ec2Connection())
            url = connection->connectionInfo().ecUrl;
        httpClient->setUserName(url.userName().toLower());
        if (ApiPeerData::isServer(localPeer().peerType))
        {
            // try auth by admin user if allowed
            QnUserResourcePtr adminUser = resPool->getAdministrator();
            if (adminUser)
            {
                httpClient->setUserPassword(adminUser->getDigest());
                httpClient->setAuthType(nx_http::AsyncClient::authDigestWithPasswordHash);
            }
        }
        else
        {
            httpClient->setUserPassword(url.password());
        }
    }
}

} // namespace p2p
} // namespace nx
