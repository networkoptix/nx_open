// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "p2p_connection.h"

#include <api/runtime_info_manager.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/metric/metrics_storage.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/nx_network_ini.h>
#include <nx/utils/log/log_main.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <nx_ec/abstract_ec_connection.h>

#include "p2p_serialization.h"

namespace nx {
namespace p2p {

Connection::Connection(
    nx::network::ssl::AdapterFunc adapterFunc,
    std::optional<nx::network::http::Credentials> credentials,
    nx::vms::common::SystemContext* systemContext,
    const nx::Uuid& remoteId,
    nx::vms::api::PeerType remotePeerType,
    const vms::api::PeerDataEx& localPeer,
    const nx::Url &remotePeerUrl,
    std::unique_ptr<QObject> opaqueObject,
    std::unique_ptr<ConnectionLockGuard> connectionLockGuard,
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
        std::move(connectionLockGuard)),
    nx::vms::common::SystemContextAware(systemContext),
    m_validateRemotePeerFunc(std::move(validateRemotePeerFunc)),
    m_credentials(std::move(credentials))
{
    nx::network::http::HttpHeaders headers;
    serializePeerData(headers, localPeer, localPeer.dataFormat);
    headers.emplace(Qn::EC2_RUNTIME_GUID_HEADER_NAME, localPeer.instanceId.toSimpleStdString());

    addAdditionalRequestHeaders(std::move(headers));

    const auto& localInfo = runtimeInfoManager()->localInfo();

    std::vector<std::pair<QString, QString>> queryParams;
    if (!localInfo.data.videoWallInstanceGuid.isNull())
    {
        queryParams.push_back({
            Qn::VIDEOWALL_INSTANCE_HEADER_NAME,
            localInfo.data.videoWallInstanceGuid.toSimpleString()});
    }

    addRequestQueryParams(std::move(queryParams));
}

Connection::Connection(
    nx::vms::common::SystemContext* systemContext,
    const vms::api::PeerDataEx& remotePeer,
    const vms::api::PeerDataEx& localPeer,
    P2pTransportPtr p2pTransport,
    const nx::network::rest::UserAccessData& userAccessData,
    const UnauthorizedWatcher& unauthorizedWatcher,
    std::unique_ptr<QObject> opaqueObject,
    std::unique_ptr<ConnectionLockGuard> connectionLockGuard)
    :
    ConnectionBase(
        remotePeer,
        localPeer,
        std::move(p2pTransport),
        std::move(opaqueObject),
        std::move(connectionLockGuard)),
    nx::vms::common::SystemContextAware(systemContext),
    m_userAccessData(userAccessData),
    m_unauthorizedWatcher(std::move(unauthorizedWatcher))
{
    systemContext->metrics()->tcpConnections().p2p()++;
}

Connection::~Connection()
{
    if (m_direction == Direction::incoming)
        systemContext()->metrics()->tcpConnections().p2p()--;
    pleaseStopSync();
}

bool Connection::fillAuthInfo(nx::network::http::AsyncClient* httpClient)
{
    using namespace nx::network::http;
    if (localPeer().isServer())
    {
        auto server = resourcePool()->getResourceById<QnMediaServerResource>(remotePeer().id);
        if (!server || server->getAuthKey().isEmpty())
            server = resourcePool()->getResourceById<QnMediaServerResource>(localPeer().id);
        if (!server)
            return false;

        auto authKey = server->getAuthKey();
        if (authKey.isEmpty())
            return false;

        httpClient->setCredentials(server->credentials());
        return true;
    }

    if (m_credentials)
    {
        httpClient->setCredentials(*m_credentials);
        return true;
    }

    return false;
}

bool Connection::validateRemotePeerData(const vms::api::PeerDataEx& remotePeer) const
{
    return m_validateRemotePeerFunc ? m_validateRemotePeerFunc(remotePeer) : true;
}

void Connection::updateCredentials(nx::network::http::Credentials credentials)
{
    m_credentials = std::move(credentials);
    m_userAccessData.setToken(m_credentials->authToken.value, /*duration*/ {});
    watchForUnauthorize();
}

void Connection::watchForUnauthorize()
{
    if (!remotePeer().isClient())
        return;

    auto weakPointer = this->weakPointer();
    NX_ASSERT(weakPointer);
    m_scopeGuards = m_unauthorizedWatcher(m_userAccessData,
        [weakPointer = std::move(weakPointer)]()
        {
            if (auto lock = weakPointer.lock())
                lock->setState(State::Unauthorized, "Unauthorized");
        });
}

} // namespace p2p
} // namespace nx
