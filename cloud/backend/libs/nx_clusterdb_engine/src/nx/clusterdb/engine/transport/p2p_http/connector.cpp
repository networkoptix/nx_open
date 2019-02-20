#include "connector.h"

#include <nx/fusion/serialization/lexical.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/websocket/websocket.h>
#include <nx/utils/log/log.h>
#include <nx/utils/uuid.h>

#include <nx/vms/api/types/connection_types.h>

#include "request_path.h"
#include "../p2p_websocket/connection.h"

namespace nx::clusterdb::engine::transport::p2p::http {

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

    if (m_commandPipeline)
        m_commandPipeline->bindToAioThread(aioThread);
}

void Connector::connect(Handler completionHandler)
{
    m_connectionId = QnUuid::createUuid().toSimpleString().toStdString();

    m_completionHandler = std::move(completionHandler);

    auto httpClient = std::make_unique<network::http::AsyncClient>();
    httpClient->addAdditionalHeader(
        Qn::EC2_CONNECTION_GUID_HEADER_NAME, m_connectionId.c_str());
    httpClient->addAdditionalHeader(
        Qn::EC2_PEER_DATA, QnUbjson::serialized(m_localPeer).toBase64());
    httpClient->addAdditionalHeader(
        Qn::EC2_RUNTIME_GUID_HEADER_NAME, m_localPeer.instanceId.toByteArray());
    // TODO: Credentials
    // httpClient->setCredentials();

    auto url = m_remoteNodeUrl;
    QUrlQuery query;
    query.addQueryItem("format", QnLexical::serialized(m_localPeer.dataFormat));
    url.setQuery(query.toString());

    m_commandPipeline = std::make_unique<nx::p2p::P2PHttpClientTransport>(
        std::move(httpClient),
        m_connectionId.c_str(),
        network::websocket::FrameType::binary,
        url);
    m_commandPipeline->bindToAioThread(getAioThread());

    m_commandPipeline->start(
        [this](auto&&... args) { handlePipelineStart(std::move(args)...); });
}

void Connector::stopWhileInAioThread()
{
    m_commandPipeline.reset();
}

void Connector::handlePipelineStart(SystemError::ErrorCode resultCode)
{
    if (resultCode != SystemError::noError)
    {
        m_commandPipeline.reset();

        NX_DEBUG(this, "Connection to %1 has failed. %2",
            m_remoteNodeUrl, SystemError::toString(resultCode));
        nx::utils::swapAndCall(m_completionHandler, resultCode, nullptr);
        return;
    }

    vms::api::PeerDataEx remotePeerData;
    // TODO: Reading remotePeerData.

    NX_DEBUG(this, "Established connection to %1", m_remoteNodeUrl);

    auto connection = std::make_unique<websocket::Connection>(
        m_protocolVersionRange,
        m_commandLog,
        m_systemId,
        m_commandFilter,
        m_connectionId,
        std::exchange(m_commandPipeline, nullptr),
        m_localPeer,
        remotePeerData);

    nx::utils::swapAndCall(m_completionHandler, resultCode, std::move(connection));
}

} // namespace nx::clusterdb::engine::transport::p2p::http
