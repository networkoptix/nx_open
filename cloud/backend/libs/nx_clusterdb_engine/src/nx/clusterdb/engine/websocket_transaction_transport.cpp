#include "websocket_transaction_transport.h"

#include <nx/p2p/p2p_serialization.h>
#include <transaction/connection_guard.h>

#include "compatible_ec2_protocol_version.h"
#include "transaction_log.h"

namespace nx::clusterdb::engine::transport {

static constexpr int kMaxTransactionsPerIteration = 17;

WebsocketCommandTransport::WebsocketCommandTransport(
    const ProtocolVersionRange& protocolVersionRange,
    CommandLog* const transactionLog,
    const std::string& systemId,
    const OutgoingCommandFilter& filter,
    const std::string& connectionId,
    nx::p2p::P2pTransportPtr _p2pTransport,
    vms::api::PeerDataEx localPeerData,
    vms::api::PeerDataEx remotePeerData)
    :
    nx::p2p::ConnectionBase(
        remotePeerData,
        localPeerData,
        std::move(_p2pTransport),
        QUrlQuery(),
        std::make_unique<nx::p2p::ConnectionContext>()),
    m_protocolVersionRange(protocolVersionRange),
    m_commonTransactionHeader(protocolVersionRange.currentVersion()),
    m_transactionLogReader(std::make_unique<CommandLogReader>(
        transactionLog,
        systemId,
        remotePeerData.dataFormat,
        filter)),
    m_systemId(systemId),
    m_connectionGuid(connectionId)
{
    bindToAioThread(p2pTransport()->getAioThread());

    m_commonTransactionHeader.systemId = systemId;
    m_commonTransactionHeader.peerId = remotePeerData.id.toSimpleByteArray().toStdString();
    m_commonTransactionHeader.endpoint = remotePeerEndpoint();
    m_commonTransactionHeader.connectionId = connectionId;
    m_commonTransactionHeader.vmsTransportHeader.sender = remotePeerData.id;
    m_commonTransactionHeader.transactionFormatVersion = remotePeerData.protoVersion;

    // #TODO #akulikov Uncomment and make it work
    //auto keepAliveTimeout = std::chrono::milliseconds(remotePeerData.aliveUpdateIntervalMs);
    //p2pTransport->setAliveTimeout(keepAliveTimeout);

    connect(this, &ConnectionBase::gotMessage, this, &WebsocketCommandTransport::onGotMessage);
    connect(this, &ConnectionBase::allDataSent,
        [this]()
        {
            if (m_remoteSubscription && !m_sendHandshakeDone && !m_tranLogRequestInProgress)
                readTransactions(); //< Continue reading
        });

    auto tranState = m_transactionLogReader->getCurrentState();
    auto serializedData = nx::p2p::serializeSubscribeAllRequest(tranState);
    serializedData.data()[0] = (quint8)(nx::p2p::MessageType::subscribeAll);
    sendMessage(serializedData);
    startReading();
}

WebsocketCommandTransport::~WebsocketCommandTransport()
{
    nx::p2p::ConnectionBase::pleaseStopSync();
}

void WebsocketCommandTransport::bindToAioThread(nx::network::aio::AbstractAioThread* aioThread)
{
    AbstractConnection::bindToAioThread(aioThread);
    nx::p2p::ConnectionBase::bindToAioThread(aioThread);
    m_transactionLogReader->bindToAioThread(aioThread);
}

void WebsocketCommandTransport::stopWhileInAioThread()
{
    AbstractConnection::stopWhileInAioThread();
    nx::p2p::ConnectionBase::stopWhileInAioThread();
    m_transactionLogReader.reset();
}

void WebsocketCommandTransport::onGotMessage(
    QWeakPointer<nx::p2p::ConnectionBase> /*connection*/,
    nx::p2p::MessageType messageType,
    const QByteArray& payload)
{
    if (!m_gotTransactionEventHandler)
        return;

    switch (messageType)
    {
        case nx::p2p::MessageType::pushTransactionData:
        {
            reportCommandReceived(payload);
            break;
        }

        case nx::p2p::MessageType::subscribeAll:
        {
            bool success = false;
            m_remoteSubscription = nx::p2p::deserializeSubscribeAllRequest(payload, &success);
            if (!success)
            {
                setState(State::Error);
                return;
            }
            readTransactions();
            break;
        }

        default:
            NX_WARNING(this, lm("P2P message type '%1' is not allowed for cloud connect! "
                "System id %2, source endpoint %3")
                .args(toString(messageType), m_systemId, remotePeerEndpoint()));
            setState(State::Error);
            break;
    }
}

void WebsocketCommandTransport::reportCommandReceived(
    QByteArray commandBuffer)
{
    CommandTransportHeader cdbTransportHeader(m_protocolVersionRange.currentVersion());
    cdbTransportHeader.endpoint = remotePeerEndpoint();
    cdbTransportHeader.systemId = m_transactionLogReader->systemId();
    cdbTransportHeader.connectionId = connectionGuid();
    //cdbTransportHeader.vmsTransportHeader //< Empty vms transport header
    cdbTransportHeader.transactionFormatVersion = highestProtocolVersionCompatibleWithRemotePeer();

    auto commandData = TransactionDeserializer::deserialize(
        remotePeer().dataFormat,
        cdbTransportHeader.peerId,
        cdbTransportHeader.transactionFormatVersion,
        std::move(commandBuffer));
    if (!commandData)
    {
        NX_DEBUG(this, lm("Failed to deserialize command from %1")
            .args(remotePeerEndpoint()));
        setState(State::Error);
        return;
    }

    m_gotTransactionEventHandler(
        std::move(commandData),
        std::move(cdbTransportHeader));
}

void WebsocketCommandTransport::readTransactions()
{
    NX_CRITICAL(m_remoteSubscription);

    m_tranLogRequestInProgress = true;

    ReadCommandsFilter filter;
    filter.from = *m_remoteSubscription;
    filter.maxTransactionsToReturn = kMaxTransactionsPerIteration;

    m_transactionLogReader->readTransactions(
        filter,
        [this](auto&&... args) { onTransactionsReadFromLog(std::move(args)...); });
}

void WebsocketCommandTransport::onTransactionsReadFromLog(
    ResultCode resultCode,
    std::vector<dao::TransactionLogRecord> serializedTransactions,
    vms::api::TranState readedUpTo)
{
    m_tranLogRequestInProgress = false;
    if ((resultCode != ResultCode::ok) && (resultCode != ResultCode::partialContent))
    {
        NX_DEBUG(this, lm("systemId %1. Error reading transaction log (%2). "
            "Closing connection to the peer %3")
            .args(m_transactionLogReader->systemId(), toString(resultCode), remotePeerEndpoint()));
        setState(State::Error);   //closing connection
        return;
    }

    // Posting transactions to send.
    for (auto& tranData: serializedTransactions)
    {
        sendMessage(
            nx::p2p::MessageType::pushTransactionData,
            tranData.serializer->serialize(
                remotePeer().dataFormat,
                highestProtocolVersionCompatibleWithRemotePeer()));
    }

    m_remoteSubscription = readedUpTo;
    if (resultCode == ResultCode::ok)
        m_sendHandshakeDone = true; //< All data are sent.
}

network::SocketAddress WebsocketCommandTransport::remotePeerEndpoint() const
{
    return p2pTransport()->getForeignAddress();
}

ConnectionClosedSubscription& WebsocketCommandTransport::connectionClosedSubscription()
{
    return m_connectionClosedSubscription;
}

void WebsocketCommandTransport::setState(State state)
{
    if (state == State::Error)
    {
        SystemError::ErrorCode errorCode = SystemError::connectionAbort;
        // TODO: pass correct error code here
        //p2pTransport()->socket()->getLastError(&errorCode);
        m_connectionClosedSubscription.notify(errorCode);
    }
    nx::p2p::ConnectionBase::setState(state);
}

void WebsocketCommandTransport::setOnGotTransaction(CommandHandler handler)
{
    m_gotTransactionEventHandler = std::move(handler);
}

std::string WebsocketCommandTransport::connectionGuid() const
{
    return m_connectionGuid;
}

const CommandTransportHeader&
    WebsocketCommandTransport::commonTransportHeaderOfRemoteTransaction() const
{
    return m_commonTransactionHeader;
}

void WebsocketCommandTransport::sendTransaction(
    CommandTransportHeader /*transportHeader*/,
    const std::shared_ptr<const SerializableAbstractCommand>& transactionSerializer)
{
    if (!m_sendHandshakeDone)
        return; //< Send data in progress. This transaction will be delivered later

    auto serializedTransaction = transactionSerializer->serialize(
        remotePeer().dataFormat,
        highestProtocolVersionCompatibleWithRemotePeer());
    sendMessage(nx::p2p::MessageType::pushTransactionData, serializedTransaction);
}

void WebsocketCommandTransport::start()
{
}

int WebsocketCommandTransport::highestProtocolVersionCompatibleWithRemotePeer() const
{
    return remotePeer().protoVersion >= m_protocolVersionRange.begin()
        ? m_protocolVersionRange.currentVersion()
        : remotePeer().protoVersion;
}

bool WebsocketCommandTransport::fillAuthInfo(
    nx::network::http::AsyncClient* /*httpClient*/,
    bool /*authByKey*/)
{
    NX_ASSERT(false, "This method is used for outgoing connections only. Not implemented");
    return false;
}

} // namespace nx::clusterdb::engine::transport
