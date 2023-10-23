// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "p2p_connection.h"

#include <api/runtime_info_manager.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/metrics/metrics_storage.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/nx_network_ini.h>
#include <nx/utils/log/log_main.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <nx_ec/abstract_ec_connection.h>

namespace nx {
namespace p2p {

Connection::Connection(
    nx::network::ssl::AdapterFunc adapterFunc,
    std::optional<nx::network::http::Credentials> credentials,
    nx::vms::common::SystemContext* systemContext,
    const QnUuid& remoteId,
    nx::vms::api::PeerType remotePeerType,
    const vms::api::PeerDataEx& localPeer,
    const utils::Url &remotePeerUrl,
    std::unique_ptr<QObject> opaqueObject,
    ConnectionLockGuard connectionLockGuard,
    ValidateRemotePeerFunc validateRemotePeerFunc)
    :
    ConnectionBase(
        remoteId,
        remotePeerType,
        localPeer,
        remotePeerUrl,
        systemContext->globalSettings()->aliveUpdateInterval(),
        std::move(opaqueObject),
        std::move(adapterFunc),
        std::make_unique<ConnectionLockGuard>(std::move(connectionLockGuard))),
    nx::vms::common::SystemContextAware(systemContext),
    m_validateRemotePeerFunc(std::move(validateRemotePeerFunc)),
    m_credentials(std::move(credentials))
{
    nx::network::http::HttpHeaders headers;
    const auto serializedPeer = localPeer.dataFormat == Qn::SerializationFormat::ubjson
        ? QnUbjson::serialized(localPeer) : QJson::serialized(localPeer);
    headers.emplace(Qn::EC2_PEER_DATA, serializedPeer.toBase64());
    headers.emplace(Qn::EC2_RUNTIME_GUID_HEADER_NAME, localPeer.instanceId.toByteArray());

    addAdditionalRequestHeaders(std::move(headers));

    const auto& localInfo = runtimeInfoManager()->localInfo();

    std::vector<std::pair<QString, QString>> queryParams;
    if (!localInfo.data.videoWallInstanceGuid.isNull())
    {
        queryParams.push_back({
            "videoWallInstanceGuid",
            localInfo.data.videoWallInstanceGuid.toSimpleString()});
    }

    addRequestQueryParams(std::move(queryParams));
}

Connection::Connection(
    nx::vms::common::SystemContext* systemContext,
    const vms::api::PeerDataEx& remotePeer,
    const vms::api::PeerDataEx& localPeer,
    P2pTransportPtr p2pTransport,
    const QUrlQuery& requestUrlQuery,
    const Qn::UserAccessData& userAccessData,
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
    nx::vms::common::SystemContextAware(systemContext),
    m_userAccessData(userAccessData)
{
    systemContext->metrics()->tcpConnections().p2p()++;
}

Connection::~Connection()
{
    if (m_direction == Direction::incoming)
        systemContext()->metrics()->tcpConnections().p2p()--;
    pleaseStopSync();
}

bool Connection::fillAuthInfo(nx::network::http::AsyncClient* httpClient, bool authByKey)
{
    using namespace nx::network::http;
    if (authByKey)
    {
        auto server = resourcePool()->getResourceById<QnMediaServerResource>(remotePeer().id);
        if (!server || server->getAuthKey().isEmpty())
            server = resourcePool()->getResourceById<QnMediaServerResource>(localPeer().id);
        if (!server)
            return false;

        auto authKey = server->getAuthKey();
        if (authKey.isEmpty())
            return false;

        httpClient->setCredentials(
            PasswordCredentials(server->getId().toStdString(), authKey.toStdString()));
        return true;
    }

    if (m_credentials)
    {
        httpClient->setCredentials(*m_credentials);
        return true;
    }

    if (NX_ASSERT(localPeer().isServer(), "Client must have credentials filled"))
    {
        // Try auth by admin user if allowed
        if (QnUserResourcePtr adminUser = resourcePool()->getAdministrator())
        {
            httpClient->setCredentials(Credentials(adminUser->getName().toStdString(),
                Ha1AuthToken(adminUser->getDigest().toStdString())));
        }
    }
    return true;
}

bool Connection::validateRemotePeerData(const vms::api::PeerDataEx& remotePeer) const
{
    return m_validateRemotePeerFunc ? m_validateRemotePeerFunc(remotePeer) : true;
}

} // namespace p2p
} // namespace nx
