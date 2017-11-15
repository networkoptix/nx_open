#include "websocket_transaction_transport.h"

#include <nx_ec/data/api_peer_data.h>
#include <nx/p2p/p2p_serialization.h>
#include <transaction/connection_guard.h>

#include "compatible_ec2_protocol_version.h"
#include "transaction_log.h"

namespace nx {
namespace cdb {
namespace ec2 {

constexpr static const int kMaxTransactionsPerIteration = 17;

WebSocketTransactionTransport::WebSocketTransactionTransport(
    nx::network::aio::AbstractAioThread* aioThread,
    TransactionLog* const transactionLog,
    const nx::String& systemId,
    const QnUuid& connectionId,
    std::unique_ptr<network::websocket::WebSocket> webSocket,
    ::ec2::ApiPeerDataEx localPeerData,
    ::ec2::ApiPeerDataEx remotePeerData)
    :
    nx::p2p::ConnectionBase(
        remotePeerData,
        localPeerData,
        std::move(webSocket),
        std::make_unique<nx::p2p::ConnectionContext>()),
    m_transactionLogReader(std::make_unique<TransactionLogReader>(
        transactionLog,
        systemId,
        remotePeerData.dataFormat)),
    m_connectionGuid(connectionId)
{
    bindToAioThread(aioThread);

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
    AbstractTransactionTransport::bindToAioThread(aioThread);
    nx::p2p::ConnectionBase::bindToAioThread(aioThread);
    m_transactionLogReader->bindToAioThread(aioThread);
}

void WebSocketTransactionTransport::stopWhileInAioThread()
{
    AbstractTransactionTransport::stopWhileInAioThread();
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
            TransactionTransportHeader cdbTransportHeader;
            cdbTransportHeader.endpoint = remoteSocketAddr();
            cdbTransportHeader.systemId = m_transactionLogReader->systemId();
            cdbTransportHeader.connectionId = connectionGuid().toSimpleByteArray();
            //cdbTransportHeader.vmsTransportHeader //< Empty vms transport header
            cdbTransportHeader.transactionFormatVersion = highestProtocolVersionCompatibleWithRemotePeer();
            m_gotTransactionEventHandler(
                remotePeer().dataFormat,
                std::move(payload),
                std::move(cdbTransportHeader));
                //cdbTransportHeader);
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
            NX_ASSERT(0, "Not implemented!");
            break;
    }
}

void WebSocketTransactionTransport::readTransactions()
{
    using namespace std::placeholders;
    m_tranLogRequestInProgress = true;
    m_transactionLogReader->readTransactions(
        m_remoteSubscription,
        boost::optional<::ec2::QnTranState>(), //< toState. Unlimited
        kMaxTransactionsPerIteration,
        std::bind(&WebSocketTransactionTransport::onTransactionsReadFromLog, this, _1, _2, _3));
}

void WebSocketTransactionTransport::onTransactionsReadFromLog(
    api::ResultCode resultCode,
    std::vector<dao::TransactionLogRecord> serializedTransactions,
    ::ec2::QnTranState readedUpTo)
{
    m_tranLogRequestInProgress = false;
    if ((resultCode != api::ResultCode::ok) && (resultCode != api::ResultCode::partialContent))
    {
        NX_DEBUG(
            this,
            lm("systemId %1. Error reading transaction log (%2). "
                "Closing connection to the peer %3")
            .arg(m_transactionLogReader->systemId())
            .arg(api::toString(resultCode))
            .arg(remoteSocketAddr()));
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
    if (resultCode == api::ResultCode::ok)
        m_sendHandshakeDone = true; //< All data are sent.
}

SocketAddress WebSocketTransactionTransport::remoteSocketAddr() const
{
    return webSocket()->socket()->getForeignAddress();
}

void WebSocketTransactionTransport::setOnConnectionClosed(ConnectionClosedEventHandler handler)
{
    m_connectionClosedEventHandler = std::move(handler);
}

void WebSocketTransactionTransport::setState(State state)
{
    if (m_connectionClosedEventHandler && state == State::Error)
    {
        SystemError::ErrorCode errorCode = SystemError::noError;
        webSocket()->socket()->getLastError(&errorCode);
        m_connectionClosedEventHandler(errorCode);
    }
    nx::p2p::ConnectionBase::setState(state);
}

void WebSocketTransactionTransport::setOnGotTransaction(GotTransactionEventHandler handler)
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

int WebSocketTransactionTransport::highestProtocolVersionCompatibleWithRemotePeer() const
{
    return remotePeer().protoVersion >= kMinSupportedProtocolVersion
        ? kMaxSupportedProtocolVersion
        : remotePeer().protoVersion;
}

void WebSocketTransactionTransport::fillAuthInfo(
    nx_http::AsyncClient* /*httpClient*/,
    bool /*authByKey*/)
{
    NX_ASSERT(0, "This method is used for outgoing connections only. Not implemented");
}

} // namespace ec2
} // namespace cdb
} // namespace nx
