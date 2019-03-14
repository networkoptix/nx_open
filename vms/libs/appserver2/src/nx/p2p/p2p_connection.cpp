#include "p2p_connection.h"

#include <common/common_module.h>
#include <api/global_settings.h>
#include <api/runtime_info_manager.h>
#include <nx/network/http/custom_headers.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <nx/utils/log/log_main.h>
#include <nx/metrics/metrics_storage.h>

namespace nx {
namespace p2p {

Connection::Connection(QnCommonModule* commonModule,
    const QnUuid& remoteId,
    const vms::api::PeerDataEx& localPeer,
    const utils::Url &remotePeerUrl,
    std::unique_ptr<QObject> opaqueObject,
    ConnectionLockGuard connectionLockGuard,
    ValidateRemotePeerFunc validateRemotePeerFunc)
    :
    ConnectionBase(
        remoteId,
        localPeer,
        remotePeerUrl,
        commonModule->globalSettings()->aliveUpdateInterval(),
        std::move(opaqueObject),
        std::make_unique<ConnectionLockGuard>(std::move(connectionLockGuard))),
    QnCommonModuleAware(commonModule),
    m_validateRemotePeerFunc(std::move(validateRemotePeerFunc))
{
    nx::network::http::HttpHeaders headers;
    headers.emplace(Qn::EC2_PEER_DATA, QnUbjson::serialized(localPeer).toBase64());
    headers.emplace(Qn::EC2_RUNTIME_GUID_HEADER_NAME, localPeer.instanceId.toByteArray());

    addAdditionalRequestHeaders(std::move(headers));

    const auto& localInfo = commonModule->runtimeInfoManager()->localInfo();

    std::vector<std::pair<QString, QString>> queryParams;
    if (!localInfo.data.videoWallInstanceGuid.isNull())
    {
        queryParams.push_back({
            "videoWallInstanceGuid",
            localInfo.data.videoWallInstanceGuid.toSimpleString()});
    }

    if (!localInfo.data.videoWallControlSession.isNull())
    {
        queryParams.push_back({
            "videoWallControlSession",
            localInfo.data.videoWallControlSession.toSimpleString()});
    }

    addRequestQueryParams(std::move(queryParams));
}

Connection::Connection(
    QnCommonModule* commonModule,
    const vms::api::PeerDataEx& remotePeer,
    const vms::api::PeerDataEx& localPeer,
    P2pTransportPtr p2pTransport,
    const QUrlQuery& requestUrlQuery,
    const Qn::UserAccessData& /*userAccessData*/,
    std::unique_ptr<QObject> opaqueObject,
    ConnectionLockGuard connectionLockGuard)
    :
    ConnectionBase(
        remotePeer,
        localPeer,
        std::move(p2pTransport),
        requestUrlQuery,
        std::move(opaqueObject),
        std::make_unique<ConnectionLockGuard>(std::move(connectionLockGuard))),
    QnCommonModuleAware(commonModule)
{
    commonModule->metrics()->tcpConnections().p2p()++;
}

Connection::~Connection()
{
    if (m_direction == Direction::incoming)
        commonModule()->metrics()->tcpConnections().p2p()--;
    pleaseStopSync();
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
            {
                httpClient->setUserCredentials(nx::network::http::Credentials(
                    adminUser->getName(),
                    nx::network::http::Ha1AuthToken(adminUser->getDigest())));
            }
        }
        else
        {
            httpClient->setUserPassword(url.password());
        }
    }
}

bool Connection::validateRemotePeerData(const vms::api::PeerDataEx& remotePeer) const
{
    return m_validateRemotePeerFunc ? m_validateRemotePeerFunc(remotePeer) : true;
}

} // namespace p2p
} // namespace nx
