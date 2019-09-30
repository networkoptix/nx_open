#include "connector.h"

#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>

#include <nx/vms/api/types/connection_types.h>

#include "command_pipeline.h"
#include "../generic_transport.h"

namespace nx::clusterdb::engine::transport::http_tunnel {

Connector::Connector(
    const ProtocolVersionRange& protocolVersionRange,
    CommandLog* commandLog,
    const OutgoingCommandFilter& outgoingCommandFilter,
    const std::string& clusterId,
    const std::string& nodeId,
    const nx::utils::Url& targetUrl)
    :
    m_protocolVersionRange(protocolVersionRange),
    m_commandLog(commandLog),
    m_outgoingCommandFilter(outgoingCommandFilter),
    m_clusterId(clusterId),
    m_nodeId(nodeId),
    m_targetUrl(targetUrl),
    m_client(std::make_unique<nx::network::http::tunneling::Client>(targetUrl))
{
    bindToAioThread(getAioThread());

    m_localPeer.id = QnUuid::fromStringSafe(nodeId.c_str());
    m_localPeer.instanceId = m_localPeer.id;
    m_localPeer.peerType = nx::vms::api::PeerType::server;
    m_localPeer.dataFormat = Qn::SerializationFormat::UbjsonFormat;
    m_localPeer.cloudHost = nx::network::SocketGlobals::cloud().cloudHost();
    m_localPeer.protoVersion = m_protocolVersionRange.currentVersion();
}

void Connector::bindToAioThread(
    network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    if (m_client)
        m_client->bindToAioThread(aioThread);
}

void Connector::connect(Handler completionHandler)
{
    m_completionHandler = std::move(completionHandler);

    m_connectionId = QnUuid::createUuid().toSimpleString().toStdString();

    ConnectionRequestAttributes connectionRequestAttributes;
    connectionRequestAttributes.connectionId = m_connectionId;
    connectionRequestAttributes.remotePeer = m_localPeer;
    connectionRequestAttributes.remotePeerProtocolVersion = m_localPeer.protoVersion;

    network::http::HttpHeaders requestHeaders;
    connectionRequestAttributes.write(&requestHeaders);
    m_client->setCustomHeaders(std::move(requestHeaders));

    m_client->openTunnel(
        [this](auto... args) { processOpenTunnelResult(std::move(args)...); });
}

void Connector::stopWhileInAioThread()
{
    m_client.reset();
}

void Connector::processOpenTunnelResult(
    nx::network::http::tunneling::OpenTunnelResult result)
{
    if (result.sysError != SystemError::noError ||
        !nx::network::http::StatusCode::isSuccessCode(result.httpStatus))
    {
        NX_DEBUG(this, "Error connecting to %1. %2, %3",
            m_targetUrl, SystemError::toString(result.sysError),
                nx::network::http::StatusCode::toString(result.httpStatus));
        return nx::utils::swapAndCall(
            m_completionHandler,
            ConnectResult(result.sysError, result.httpStatus),
            nullptr);
    }

    ConnectionRequestAttributes remotePeerAttributes;
    if (!remotePeerAttributes.read(m_client->response().headers))
    {
        NX_DEBUG(this, "Error connecting to %1. Could not read attributes from the response",
            m_targetUrl);
        return nx::utils::swapAndCall(
            m_completionHandler,
            ConnectResult(SystemError::noError, network::http::StatusCode::badRequest),
            nullptr);
    }
    remotePeerAttributes.connectionId = m_connectionId;

    auto connectionId = QnUuid::createUuid().toSimpleByteArray().toStdString();

    auto commandPipe = std::make_unique<CommandPipeline>(
        m_protocolVersionRange,
        remotePeerAttributes,
        m_clusterId,
        std::exchange(result.connection, nullptr));

    auto connection = std::make_unique<GenericTransport>(
        m_protocolVersionRange,
        m_commandLog,
        m_outgoingCommandFilter,
        m_clusterId,
        remotePeerAttributes,
        m_localPeer,
        std::move(commandPipe));

    nx::utils::swapAndCall(
        m_completionHandler,
        ConnectResult(SystemError::noError),
        std::move(connection));
}

} // namespace nx::clusterdb::engine::transport::http_tunnel
