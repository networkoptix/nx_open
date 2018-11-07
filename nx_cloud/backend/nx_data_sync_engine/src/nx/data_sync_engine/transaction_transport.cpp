#include "transaction_transport.h"

#include <nx/fusion/serialization/lexical.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include "compatible_ec2_protocol_version.h"
#include "command_descriptor.h"

namespace nx::data_sync_engine::transport {

constexpr static const std::chrono::seconds kTcpKeepAliveTimeout = std::chrono::seconds(5);
constexpr static const int kKeepAliveProbeCount = 3;

TransactionTransport::TransactionTransport(
    const ProtocolVersionRange& protocolVersionRange,
    nx::network::aio::AbstractAioThread* aioThread,
    std::shared_ptr<::ec2::ConnectionGuardSharedState> connectionGuardSharedState,
    const transport::ConnectionRequestAttributes& connectionRequestAttributes,
    const std::string& systemId,
    const vms::api::PeerData& localPeer,
    const network::SocketAddress& remotePeerEndpoint,
    const nx::network::http::Request& request)
    :
    TransactionTransport(
        protocolVersionRange,
        std::make_unique<::ec2::QnTransactionTransportBase>(
            QnUuid(), //< localSystemId. Not used here
            QnUuid::fromStringSafe(connectionRequestAttributes.connectionId),
            ::ec2::ConnectionLockGuard(
                localPeer.id,
                connectionGuardSharedState.get(),
                connectionRequestAttributes.remotePeer.id,
                ::ec2::ConnectionLockGuard::Direction::Incoming),
            localPeer,
            connectionRequestAttributes.remotePeer,
            ::ec2::ConnectionType::incoming,
            request,
            connectionRequestAttributes.contentEncoding.c_str(),
            kTcpKeepAliveTimeout,
            kKeepAliveProbeCount),
        connectionRequestAttributes.connectionId,
        aioThread,
        connectionGuardSharedState,
        systemId,
        remotePeerEndpoint)
{
}

TransactionTransport::TransactionTransport(
    const ProtocolVersionRange& protocolVersionRange,
    std::unique_ptr<::ec2::QnTransactionTransportBase> connection,
    std::shared_ptr<::ec2::ConnectionGuardSharedState> connectionGuardSharedState,
    const std::string& systemId,
    const network::SocketAddress& remotePeerEndpoint)
    :
    TransactionTransport(
        protocolVersionRange,
        std::move(connection),
        connection->connectionGuid().toSimpleByteArray().toStdString(),
        connection->getAioThread(),
        connectionGuardSharedState,
        systemId,
        remotePeerEndpoint)
{
}

TransactionTransport::TransactionTransport(
    const ProtocolVersionRange& protocolVersionRange,
    std::unique_ptr<::ec2::QnTransactionTransportBase> connection,
    const std::string& connectionId,
    nx::network::aio::AbstractAioThread* aioThread,
    std::shared_ptr<::ec2::ConnectionGuardSharedState> connectionGuardSharedState,
    const std::string& systemId,
    const network::SocketAddress& remotePeerEndpoint)
    :
    m_protocolVersionRange(protocolVersionRange),
    m_connectionGuardSharedState(connectionGuardSharedState),
    m_baseTransactionTransport(std::move(connection)),
    m_systemId(systemId),
    m_connectionId(connectionId),
    m_connectionOriginatorEndpoint(remotePeerEndpoint),
    m_inactivityTimer(std::make_unique<network::aio::Timer>())
{
    bindToAioThread(aioThread);
    m_baseTransactionTransport->setState(::ec2::QnTransactionTransportBase::ReadyForStreaming);
    // Ignoring "state changed to Connected" signal.

    QObject::connect(
        m_baseTransactionTransport.get(), &::ec2::QnTransactionTransportBase::gotTransaction,
        this, &TransactionTransport::onGotTransaction,
        Qt::DirectConnection);

    QObject::connect(
        m_baseTransactionTransport.get(), &::ec2::QnTransactionTransportBase::stateChanged,
        this, &TransactionTransport::onStateChanged,
        Qt::DirectConnection);

    QObject::connect(
        m_baseTransactionTransport.get(), &::ec2::QnTransactionTransportBase::onSomeDataReceivedFromRemotePeer,
        this, &TransactionTransport::restartInactivityTimer,
        Qt::DirectConnection);

    if (m_baseTransactionTransport->remotePeerSupportsKeepAlive())
    {
        m_inactivityTimer->start(
            m_baseTransactionTransport->connectionKeepAliveTimeout()
                * m_baseTransactionTransport->keepAliveProbeCount(),
            [this]() { onInactivityTimeout(); });
    }
}

TransactionTransport::~TransactionTransport()
{
    stopWhileInAioThread();
}

void TransactionTransport::bindToAioThread(
    nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_baseTransactionTransport->bindToAioThread(aioThread);
    m_inactivityTimer->bindToAioThread(aioThread);
}

void TransactionTransport::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_baseTransactionTransport->stopWhileInAioThread();
    m_inactivityTimer.reset();
}

network::SocketAddress TransactionTransport::remotePeerEndpoint() const
{
    return m_connectionOriginatorEndpoint;
}

void TransactionTransport::setOnConnectionClosed(ConnectionClosedEventHandler handler)
{
    m_connectionClosedEventHandler = std::move(handler);
}

void TransactionTransport::setOnGotTransaction(GotTransactionEventHandler handler)
{
    m_gotTransactionEventHandler = std::move(handler);
}

QnUuid TransactionTransport::connectionGuid() const
{
    return m_baseTransactionTransport->connectionGuid();
}

void TransactionTransport::sendTransaction(
    TransactionTransportHeader transportHeader,
    const std::shared_ptr<const TransactionSerializer>& transactionSerializer)
{
    transportHeader.vmsTransportHeader.fillSequence(
        m_baseTransactionTransport->localPeer().id,
        m_baseTransactionTransport->localPeer().instanceId);
    auto serializedTransaction = transactionSerializer->serialize(
        m_baseTransactionTransport->remotePeer().dataFormat,
        std::move(transportHeader),
        highestProtocolVersionCompatibleWithRemotePeer());

    post(
        [this, serializedTransaction = std::move(serializedTransaction)]()
        {
            m_baseTransactionTransport->addDataToTheSendQueue(
                std::move(serializedTransaction));
        });
}

void TransactionTransport::closeConnection()
{
    m_baseTransactionTransport->setState(
        ::ec2::QnTransactionTransportBase::Closed);
}

void TransactionTransport::receivedTransaction(
    const nx::network::http::HttpHeaders& headers,
    const QnByteArrayConstRef& tranData)
{
    m_baseTransactionTransport->receivedTransaction(headers, tranData);
}

void TransactionTransport::setOutgoingConnection(
    std::unique_ptr<network::AbstractCommunicatingSocket> socket)
{
    m_baseTransactionTransport->setOutgoingConnection(std::move(socket));
}

nx::vms::api::PeerData TransactionTransport::localPeer() const
{
    return m_baseTransactionTransport->localPeer();
}

nx::vms::api::PeerData TransactionTransport::remotePeer() const
{
    return m_baseTransactionTransport->remotePeer();
}

int TransactionTransport::remotePeerProtocolVersion() const
{
    return m_baseTransactionTransport->remotePeerProtocolVersion();
}

int TransactionTransport::highestProtocolVersionCompatibleWithRemotePeer() const
{
    return m_baseTransactionTransport->remotePeerProtocolVersion() >= m_protocolVersionRange.begin()
        ? m_protocolVersionRange.currentVersion()
        : m_baseTransactionTransport->remotePeerProtocolVersion();
}

void TransactionTransport::onGotTransaction(
    Qn::SerializationFormat tranFormat,
    QByteArray data,
    CommandTransportHeader transportHeader)
{
    NX_CRITICAL(isInSelfAioThread());

    // ::ec2::QnTransactionTransportBase::gotTransaction allows binding to
    //  itself only as QueuedConnection, so we have to use post to exit this handler ASAP.

    post(
        [this,
            tranFormat,
            data = std::move(data),
            transportHeader = std::move(transportHeader)]() mutable
        {
            forwardTransactionToProcessor(
                tranFormat,
                std::move(data),
                std::move(transportHeader));
        });
}

void TransactionTransport::forwardTransactionToProcessor(
    Qn::SerializationFormat tranFormat,
    QByteArray data,
    CommandTransportHeader transportHeader)
{
    NX_CRITICAL(isInSelfAioThread());

    if (!m_gotTransactionEventHandler)
        return;

    if (m_closed)
    {
        NX_VERBOSE(this, lm("systemId %1. Received transaction from %2 after connection closure")
            .args(m_systemId, m_connectionOriginatorEndpoint));
        return;
    }

    TransactionTransportHeader cdbTransportHeader(m_protocolVersionRange.currentVersion());
    cdbTransportHeader.endpoint = m_connectionOriginatorEndpoint;
    cdbTransportHeader.systemId = m_systemId;
    cdbTransportHeader.connectionId = m_connectionId;
    cdbTransportHeader.vmsTransportHeader = std::move(transportHeader);
    cdbTransportHeader.transactionFormatVersion = highestProtocolVersionCompatibleWithRemotePeer();
    m_gotTransactionEventHandler(
        tranFormat,
        std::move(data),
        std::move(cdbTransportHeader));
}

void TransactionTransport::onStateChanged(
    ::ec2::QnTransactionTransportBase::State newState)
{
    NX_CRITICAL(isInSelfAioThread());

    // ::ec2::QnTransactionTransportBase::gotTransaction allows binding to
    //  itself only as QueuedConnection, so we have to use post to exit this handler ASAP.

    post(
        [this, newState]() mutable
        {
            forwardStateChangedEvent(newState);
        });
}

void TransactionTransport::forwardStateChangedEvent(
    ::ec2::QnTransactionTransportBase::State newState)
{
    NX_CRITICAL(isInSelfAioThread());

    if (m_closed)
        return; //< Not reporting multiple close events.

    if (newState == ::ec2::QnTransactionTransportBase::Closed ||
        newState == ::ec2::QnTransactionTransportBase::Error)
    {
        NX_VERBOSE(QnLog::EC2_TRAN_LOG.join(this),
            lm("systemId %1, connection %2. Reporting connection closure")
                .args(m_systemId, m_connectionId));

        m_closed = true;
        if (m_connectionClosedEventHandler)
            m_connectionClosedEventHandler(SystemError::connectionReset);
    }
}

void TransactionTransport::restartInactivityTimer()
{
    NX_CRITICAL(isInSelfAioThread());
    NX_CRITICAL(m_inactivityTimer->isInSelfAioThread());

    m_inactivityTimer->cancelSync();
    m_inactivityTimer->start(
        m_baseTransactionTransport->connectionKeepAliveTimeout()
            * m_baseTransactionTransport->keepAliveProbeCount(),
        std::bind(&TransactionTransport::onInactivityTimeout, this));
}

void TransactionTransport::onInactivityTimeout()
{
    NX_VERBOSE(QnLog::EC2_TRAN_LOG.join(this), lm("systemId %1, connection %2. Inactivity timeout triggered")
        .args(m_systemId, m_connectionId));
    m_baseTransactionTransport->setState(::ec2::QnTransactionTransportBase::Error);
}

} // namespace nx::data_sync_engine::transport
