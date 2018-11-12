#include "websocket_transaction_transport.h"

#include <nx/p2p/p2p_serialization.h>
#include <transaction/connection_guard.h>

#include "compatible_ec2_protocol_version.h"
#include "transaction_log.h"

namespace nx::data_sync_engine::transport {

static constexpr int kMaxTransactionsPerIteration = 17;

WebSocketTransactionTransport::WebSocketTransactionTransport(
    const ProtocolVersionRange& protocolVersionRange,
    TransactionLog* const transactionLog,
    const std::string& systemId,
    const OutgoingCommandFilter& filter,
    const QnUuid& connectionId,
    std::unique_ptr<network::websocket::WebSocket> webSocket,
    vms::api::PeerDataEx localPeerData,
    vms::api::PeerDataEx remotePeerData)
    :
    nx::p2p::ConnectionBase(
        remotePeerData,
        localPeerData,
        std::move(webSocket),
        QUrlQuery(),
        std::make_unique<nx::p2p::ConnectionContext>()),
    m_protocolVersionRange(protocolVersionRange),
    m_commonTransactionHeader(protocolVersionRange.currentVersion()),
    m_transactionLogReader(std::make_unique<TransactionLogReader>(
        transactionLog,
        systemId,
        remotePeerData.dataFormat,
        filter)),
    m_systemId(systemId),
    m_connectionGuid(connectionId)
{
    bindToAioThread(this->webSocket()->getAioThread());

    m_commonTransactionHeader.systemId = systemId;
    m_commonTransactionHeader.peerId = remotePeerData.id.toSimpleByteArray().toStdString();
    m_commonTransactionHeader.endpoint = remotePeerEndpoint();
    m_commonTransactionHeader.connectionId = connectionId.toSimpleString().toStdString();
    m_commonTransactionHeader.vmsTransportHeader.sender = remotePeerData.id;
    m_commonTransactionHeader.transactionFormatVersion = remotePeerData.protoVersion;

    auto keepAliveTimeout = std::chrono::milliseconds(remotePeerData.aliveUpdateIntervalMs);
    this->webSocket()->setAliveTimeout(keepAliveTimeout);

    connect(this, &ConnectionBase::gotMessage, this, &WebSocketTransactionTransport::onGotMessage);
    connect(this, &ConnectionBase::allDataSent,
        [this]()
        {
            if (!m_sendHandshakeDone && !m_tranLogRequestInProgress)
                readTransactions(); //< Continue reading
        });

    auto tranState = m_transactionLogReader->getCurrentState();
    auto serializedData = nx::p2p::serializeSubscribeAllRequest(tranState);
    serializedData.data()[0] = (quint8)(nx::p2p::MessageType::subscribeAll);
    sendMessage(serializedData);
    startReading();
}

void WebSocketTransactionTransport::bindToAioThread(nx::network::aio::AbstractAioThread* aioThread)
{
    AbstractConnection::bindToAioThread(aioThread);
    nx::p2p::ConnectionBase::bindToAioThread(aioThread);
    m_transactionLogReader->bindToAioThread(aioThread);
}

void WebSocketTransactionTransport::stopWhileInAioThread()
{
    AbstractConnection::stopWhileInAioThread();
    nx::p2p::ConnectionBase::stopWhileInAioThread();
    m_transactionLogReader.reset();
}

void WebSocketTransactionTransport::onGotMessage(
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

void WebSocketTransactionTransport::reportCommandReceived(
    QByteArray commandBuffer)
{
    TransactionTransportHeader cdbTransportHeader(m_protocolVersionRange.currentVersion());
    cdbTransportHeader.endpoint = remotePeerEndpoint();
    cdbTransportHeader.systemId = m_transactionLogReader->systemId();
    cdbTransportHeader.connectionId = connectionGuid().toSimpleByteArray().toStdString();
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

void WebSocketTransactionTransport::readTransactions()
{
    using namespace std::placeholders;
    m_tranLogRequestInProgress = true;
    
    ReadCommandsFilter filter;
    filter.from = m_remoteSubscription;
    filter.maxTransactionsToReturn = kMaxTransactionsPerIteration;

    m_transactionLogReader->readTransactions(
        filter,
        std::bind(&WebSocketTransactionTransport::onTransactionsReadFromLog, this, _1, _2, _3));
}

void WebSocketTransactionTransport::onTransactionsReadFromLog(
    ResultCode resultCode,
    std::vector<dao::TransactionLogRecord> serializedTransactions,
    vms::api::TranState readedUpTo)
{
    m_tranLogRequestInProgress = false;
    if ((resultCode != ResultCode::ok) && (resultCode != ResultCode::partialContent))
    {
        NX_DEBUG(
            this,
            lm("systemId %1. Error reading transaction log (%2). "
                "Closing connection to the peer %3")
            .args(m_transactionLogReader->systemId(),
                toString(resultCode), remotePeerEndpoint()));
        setState(State::Error);   //closing connection
        return;
    }

    // Posting transactions to send
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

network::SocketAddress WebSocketTransactionTransport::remotePeerEndpoint() const
{
    return webSocket()->socket()->getForeignAddress();
}

ConnectionClosedSubscription& WebSocketTransactionTransport::connectionClosedSubscription()
{
    return m_connectionClosedSubscription;
}

void WebSocketTransactionTransport::setState(State state)
{
    if (state == State::Error)
    {
        SystemError::ErrorCode errorCode = SystemError::noError;
        webSocket()->socket()->getLastError(&errorCode);
        m_connectionClosedSubscription.notify(errorCode);
    }
    nx::p2p::ConnectionBase::setState(state);
}

void WebSocketTransactionTransport::setOnGotTransaction(CommandHandler handler)
{
    m_gotTransactionEventHandler = std::move(handler);
}

QnUuid WebSocketTransactionTransport::connectionGuid() const
{
    return m_connectionGuid;
}

const TransactionTransportHeader&
    WebSocketTransactionTransport::commonTransportHeaderOfRemoteTransaction() const
{
    return m_commonTransactionHeader;
}

void WebSocketTransactionTransport::sendTransaction(
    TransactionTransportHeader /*transportHeader*/,
    const std::shared_ptr<const SerializableAbstractTransaction>& transactionSerializer)
{
    if (!m_sendHandshakeDone)
        return; //< Send data in progress. This transaction will be delivered later

    auto serializedTransaction = transactionSerializer->serialize(
        remotePeer().dataFormat,
        highestProtocolVersionCompatibleWithRemotePeer());
    sendMessage(nx::p2p::MessageType::pushTransactionData, serializedTransaction);
}

void WebSocketTransactionTransport::start()
{
}

int WebSocketTransactionTransport::highestProtocolVersionCompatibleWithRemotePeer() const
{
    return remotePeer().protoVersion >= m_protocolVersionRange.begin()
        ? m_protocolVersionRange.currentVersion()
        : remotePeer().protoVersion;
}

void WebSocketTransactionTransport::fillAuthInfo(
    nx::network::http::AsyncClient* /*httpClient*/,
    bool /*authByKey*/)
{
    NX_ASSERT(0, "This method is used for outgoing connections only. Not implemented");
}

} // namespace nx::data_sync_engine::transport
