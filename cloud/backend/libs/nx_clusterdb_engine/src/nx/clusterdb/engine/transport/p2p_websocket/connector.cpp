#include "connector.h"

#include <nx/fusion/serialization/lexical.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/websocket/websocket.h>
#include <nx/network/websocket/websocket_handshake.h>
#include <nx/utils/log/log.h>
#include <nx/utils/uuid.h>

#include <nx/p2p/p2p_serialization.h>
#include <nx/vms/api/types/connection_types.h>

#include "request_path.h"
#include "../p2p_websocket/connection.h"

namespace nx::clusterdb::engine::transport::p2p::websocket {

static constexpr int kMaxStage2Tries = 2;

Connector::Connector(
    const ProtocolVersionRange& protocolVersionRange,
    CommandLog* commandLog,
    const OutgoingCommandFilter& commandFilter,
    const nx::utils::Url& remoteNodeUrl,
    const std::string& systemId,
    const std::string& nodeId)
    :
    m_protocolVersionRange(protocolVersionRange),
    m_commandLog(commandLog),
    m_commandFilter(commandFilter),
    m_remoteNodeUrl(nx::network::url::Builder(remoteNodeUrl).appendPath(kCommandPath)),
    m_systemId(systemId)
{
    m_localPeer.id = QnUuid::fromStringSafe(nodeId.c_str());
    m_localPeer.instanceId = m_localPeer.id;
    m_localPeer.peerType = nx::vms::api::PeerType::server;
    m_localPeer.dataFormat = Qn::SerializationFormat::UbjsonFormat;
    m_localPeer.cloudHost = nx::network::SocketGlobals::cloud().cloudHost();
    m_localPeer.protoVersion = m_protocolVersionRange.currentVersion();
}

void Connector::bindToAioThread(network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    if (m_connectionUpgradeClient)
        m_connectionUpgradeClient->bindToAioThread(aioThread);

    if (m_commandPipeline)
        m_commandPipeline->bindToAioThread(aioThread);
}

void Connector::connect(Handler completionHandler)
{
    m_connectionId = QnUuid::createUuid().toSimpleString().toStdString();

    m_completionHandler = std::move(completionHandler);

    m_connectionUpgradeClient = std::make_unique<network::http::AsyncClient>();
    m_connectionUpgradeClient->bindToAioThread(getAioThread());
    m_connectionUpgradeClient->addAdditionalHeader(
        Qn::EC2_CONNECTION_GUID_HEADER_NAME, m_connectionId.c_str());
    m_connectionUpgradeClient->addAdditionalHeader(
        Qn::EC2_PEER_DATA, QnUbjson::serialized(m_localPeer).toBase64());
    m_connectionUpgradeClient->addAdditionalHeader(
        Qn::EC2_RUNTIME_GUID_HEADER_NAME, m_localPeer.instanceId.toByteArray());

    nx::network::websocket::addClientHeaders(
        m_connectionUpgradeClient.get(),
        nx::network::websocket::kWebsocketProtocolName);

    auto url = m_remoteNodeUrl;
    QUrlQuery query;
    query.addQueryItem("format", QnLexical::serialized(m_localPeer.dataFormat));
    url.setQuery(query.toString());

    m_connectionUpgradeClient->doUpgrade(
        url,
        nx::network::http::Method::get,
        nx::network::websocket::kWebsocketProtocolName,
        [this]() { handleUpgradeCompletion(); });
}

void Connector::stopWhileInAioThread()
{
    m_connectionUpgradeClient.reset();
    m_commandPipeline.reset();
}

void Connector::handleUpgradeCompletion()
{
    if (m_connectionUpgradeClient->failed())
    {
        auto connectionUpgradeClient = std::exchange(m_connectionUpgradeClient, nullptr);

        NX_DEBUG(this, "Connection to %1 has failed. %2",
            m_remoteNodeUrl, SystemError::toString(connectionUpgradeClient->lastSysErrorCode()));
        nx::utils::swapAndCall(m_completionHandler, SystemError::connectionRefused, nullptr);
        return;
    }

    if (m_connectionUpgradeClient->response()->statusLine.statusCode ==
        nx::network::http::StatusCode::switchingProtocols)
    {
        upgradeHttpConnectionToCommandTransportConnection();
    }
    else if (m_connectionUpgradeClient->response()->statusLine.statusCode ==
        nx::network::http::StatusCode::noContent &&
        m_connectionUpgradeClient->response()->headers.count(Qn::EC2_CONNECT_STAGE_1) > 0 &&
        m_stage2TryCount < kMaxStage2Tries)
    {
        sendConnectStage2Request();
        ++m_stage2TryCount;
    }
    else
    {
        m_connectionUpgradeClient.reset();
        NX_DEBUG(this, "Connection to %1 has failed. %2 (%3) received",
            m_remoteNodeUrl, m_connectionUpgradeClient->response()->statusLine.statusCode,
            m_connectionUpgradeClient->response()->statusLine.reasonPhrase);
        nx::utils::swapAndCall(m_completionHandler, SystemError::connectionRefused, nullptr);
    }
}

void Connector::upgradeHttpConnectionToCommandTransportConnection()
{
    const auto resultCode = nx::network::websocket::validateResponse(
        m_connectionUpgradeClient->request(),
        *m_connectionUpgradeClient->response());
    if (resultCode != nx::network::websocket::Error::noError)
    {
        NX_DEBUG(this, "Failed to validate websocket upgrade to %1. Error: %2",
            m_remoteNodeUrl, toString(resultCode));
        nx::utils::swapAndCall(m_completionHandler, SystemError::connectionRefused, nullptr);
        return;
    }

    m_remotePeerData = nx::p2p::deserializePeerData(
        m_connectionUpgradeClient->response()->headers, m_localPeer.dataFormat);

    NX_DEBUG(this, "Upgraded connection to %1 to websocket", m_remoteNodeUrl);

    m_commandPipeline = std::make_unique<nx::p2p::P2PWebsocketTransport>(
        m_connectionUpgradeClient->takeSocket(),
        nx::network::websocket::FrameType::binary);
    m_commandPipeline->bindToAioThread(getAioThread());

    m_connectionUpgradeClient.reset();

    m_commandPipeline->start(
        [this](auto resultCode) { handlePipelineStart(resultCode); });
}

void Connector::sendConnectStage2Request()
{
    NX_VERBOSE(this, "Sending connect stage 2 request to %1", m_remoteNodeUrl);

    m_connectionUpgradeClient->doUpgrade(
        m_connectionUpgradeClient->url(),
        nx::network::http::Method::get,
        nx::network::websocket::kWebsocketProtocolName,
        [this]() { handleUpgradeCompletion(); });
}

void Connector::handlePipelineStart(SystemError::ErrorCode resultCode)
{
    NX_DEBUG(this, "Command pipeline to %1 start completed with result %2",
        m_remoteNodeUrl, SystemError::toString(resultCode));

    if (resultCode != SystemError::noError)
    {
        nx::utils::swapAndCall(m_completionHandler, resultCode, nullptr);
        return;
    }

    auto connection = std::make_unique<websocket::Connection>(
        m_protocolVersionRange,
        m_commandLog,
        m_systemId,
        m_commandFilter,
        m_connectionId,
        std::exchange(m_commandPipeline, nullptr),
        m_localPeer,
        m_remotePeerData);

    nx::utils::swapAndCall(m_completionHandler, SystemError::noError, std::move(connection));
}

} // namespace nx::clusterdb::engine::transport::p2p::websocket
