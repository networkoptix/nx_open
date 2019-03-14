#include "connector.h"

#include <nx/network/url/url_builder.h>
#include <nx/network/url/url_parse_helper.h>

#include <nx/vms/api/types/connection_types.h>

#include "request_path.h"
#include "connection.h"
#include "../generic_transport.h"

namespace nx::clusterdb::engine::transport {

static constexpr auto kTcpKeepAliveTimeout = std::chrono::seconds(7);
static constexpr int kKeepAliveProbeCount = 3;
static constexpr char kHttpTransportPathBase[] = "/events";

HttpCommandPipelineConnector::HttpCommandPipelineConnector(
    const ProtocolVersionRange& protocolVersionRange,
    const nx::utils::Url& nodeUrl,
    const std::string& systemId,
    const std::string& nodeId)
    :
    m_protocolVersionRange(protocolVersionRange),
    m_getCommandsNodeUrl(nx::network::url::Builder(nodeUrl)
        .appendPath(kHttpTransportPathBase)
        .setQuery(lm("guid=%1").args(nodeId)).toUrl()),
    m_postCommandsNodeUrl(nx::network::url::Builder(nodeUrl)
        .appendPath(kPushEc2TransactionPathWithoutSequence)),
    m_systemId(systemId),
    m_connectionGuardSharedState(std::make_shared<::ec2::ConnectionGuardSharedState>())
{
    m_peerData.id = QnUuid::fromStringSafe(nodeId.c_str());
    m_peerData.persistentId = QnUuid::fromArbitraryData(systemId);
    m_peerData.instanceId = m_peerData.id;
    m_peerData.peerType = nx::vms::api::PeerType::server;
    m_peerData.dataFormat = Qn::SerializationFormat::UbjsonFormat;
}

void HttpCommandPipelineConnector::bindToAioThread(
    network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_connection->bindToAioThread(aioThread);
}

void HttpCommandPipelineConnector::connect(Handler completionHandler)
{
    m_completionHandler = std::move(completionHandler);

    m_connection = std::make_unique<::ec2::QnTransactionTransportBase>(
        QnUuid::fromStringSafe(m_systemId),
        &*m_connectionGuardSharedState,
        m_peerData,
        kTcpKeepAliveTimeout,
        kKeepAliveProbeCount);

    m_connection->bindToAioThread(getAioThread());

    m_stateChangedConnection = QObject::connect(
        m_connection.get(), &::ec2::QnTransactionTransportBase::stateChanged,
        [this](auto&&... args) { onStateChanged(std::move(args)...); });

    m_connection->doOutgoingConnect(m_getCommandsNodeUrl);
}

void HttpCommandPipelineConnector::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_connection.reset();
}

void HttpCommandPipelineConnector::onStateChanged(
    ::ec2::QnTransactionTransportBase::State newState)
{
    using State = ::ec2::QnTransactionTransportBase::State;

    // TODO: Have to do post here since onStateChanged is called by
    // QnTransactionTransportBase with mutex locked.
    post(
        [this, newState]()
        {
            switch (newState)
            {
                case State::Connected:
                case State::NeedStartStreaming:
                case State::ReadyForStreaming:
                    processSuccessfulConnect();
                    break;

                case State::Closed:
                case State::Error:
                    processConnectFailure();
                    break;

                default:
                    // An intermediate state. Ignoring...
                    return;
            }
        });
}

void HttpCommandPipelineConnector::processSuccessfulConnect()
{
    // TODO: #ak ConnectionGuardSharedState MUST be shared with Acceptor.

    m_connection->disconnect(m_stateChangedConnection);

    m_connection->setPostTranUrl(m_postCommandsNodeUrl);

    auto commandPipeline = std::make_unique<CommonHttpConnection>(
        m_protocolVersionRange,
        std::exchange(m_connection, nullptr),
        //getAioThread(),
        m_connectionGuardSharedState,
        m_systemId,
        nx::network::url::getEndpoint(m_getCommandsNodeUrl));

    nx::utils::swapAndCall(
        m_completionHandler,
        ConnectResultDescriptor(),
        std::move(commandPipeline));
}

void HttpCommandPipelineConnector::processConnectFailure()
{
    m_connection.reset();

    nx::utils::swapAndCall(
        m_completionHandler,
        ConnectResultDescriptor(SystemError::connectionRefused),
        nullptr);
}

//-------------------------------------------------------------------------------------------------

HttpTransportConnector::HttpTransportConnector(
    const ProtocolVersionRange& protocolVersionRange,
    CommandLog* transactionLog,
    const OutgoingCommandFilter& outgoingCommandFilter,
    const nx::utils::Url& nodeUrl,
    const std::string& systemId,
    const std::string& nodeId)
    :
    m_protocolVersionRange(protocolVersionRange),
    m_commandLog(transactionLog),
    m_outgoingCommandFilter(outgoingCommandFilter),
    m_systemId(systemId),
    m_pipelineConnector(
        protocolVersionRange,
        nodeUrl,
        systemId,
        nodeId)
{
}

void HttpTransportConnector::bindToAioThread(
    network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_pipelineConnector.bindToAioThread(aioThread);
}

void HttpTransportConnector::connect(Handler completionHandler)
{
    m_completionHandler = std::move(completionHandler);

    m_pipelineConnector.connect(
        [this](auto&&... args) { onPipelineConnectCompleted(std::move(args)...); });
}

void HttpTransportConnector::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_pipelineConnector.pleaseStopSync();
}

void HttpTransportConnector::onPipelineConnectCompleted(
    ConnectResultDescriptor connectResultDescriptor,
    std::unique_ptr<AbstractCommandPipeline> connection)
{
    if (!connection)
    {
        nx::utils::swapAndCall(
            m_completionHandler,
            std::move(connectResultDescriptor),
            nullptr);
        return;
    }

    CommonHttpConnection* transport =
        static_cast<CommonHttpConnection*>(connection.get());

    ConnectionRequestAttributes connectionRequestAttributes;
    connectionRequestAttributes.remotePeer = transport->remotePeer();
    connectionRequestAttributes.remotePeerProtocolVersion =
        transport->remotePeerProtocolVersion();
    connectionRequestAttributes.connectionId = transport->connectionGuid();

    auto newTransport = std::make_unique<GenericTransport>(
        m_protocolVersionRange,
        m_commandLog,
        m_outgoingCommandFilter,
        m_systemId,
        connectionRequestAttributes,
        transport->localPeer(),
        std::move(connection));

    nx::utils::swapAndCall(
        m_completionHandler,
        ConnectResultDescriptor(),
        std::move(newTransport));
}

} // namespace nx::clusterdb::engine::transport
