#include "p2p_connection.h"

#include <common/common_module.h>
#include <api/global_settings.h>
#include <nx/network/http/custom_headers.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>

namespace nx {
namespace p2p {

Connection::Connection(QnCommonModule* commonModule,
    const QnUuid& remoteId,
    const vms::api::PeerDataEx& localPeer,
    const utils::Url &remotePeerUrl,
    std::unique_ptr<QObject> opaqueObject,
    ConnectionLockGuard connectionLockGuard)
    :
    ConnectionBase(
        remoteId,
        localPeer,
        remotePeerUrl,
        commonModule->globalSettings()->aliveUpdateInterval(),
        std::move(opaqueObject),
        std::make_unique<ConnectionLockGuard>(std::move(connectionLockGuard))),
    QnCommonModuleAware(commonModule)
{
}

Connection::Connection(
    QnCommonModule* commonModule,
    const vms::api::PeerDataEx& remotePeer,
    const vms::api::PeerDataEx& localPeer,
    nx::network::WebSocketPtr webSocket,
    const Qn::UserAccessData& userAccessData,
    std::unique_ptr<QObject> opaqueObject,
    ConnectionLockGuard connectionLockGuard)
    :
    ConnectionBase(
        remotePeer,
        localPeer,
        std::move(webSocket),
        std::move(opaqueObject),
        std::make_unique<ConnectionLockGuard>(std::move(connectionLockGuard))),
    QnCommonModuleAware(commonModule)
{
}

void Connection::fillAuthInfo(nx::network::http::AsyncClient* httpClient, bool authByKey)
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
        nx::utils::Url url;
        if (const auto& connection = commonModule()->ec2Connection())
            url = connection->connectionInfo().ecUrl;
        httpClient->setUserName(url.userName().toLower());
        if (vms::api::PeerData::isServer(localPeer().peerType))
        {
            // try auth by admin user if allowed
            QnUserResourcePtr adminUser = resPool->getAdministrator();
            if (adminUser)
                httpClient->setUserAuthToken(nx::network::http::Ha1AuthToken(adminUser->getDigest()));
        }
        else
        {
            httpClient->setUserPassword(url.password());
        }
    }
}

} // namespace p2p
} // namespace nx
