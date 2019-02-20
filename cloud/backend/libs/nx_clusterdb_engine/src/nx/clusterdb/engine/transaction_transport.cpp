#include "transaction_transport.h"

#include <nx/fusion/serialization/lexical.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include "compatible_ec2_protocol_version.h"
#include "command_descriptor.h"

namespace nx::clusterdb::engine::transport {

constexpr static const std::chrono::seconds kTcpKeepAliveTimeout = std::chrono::seconds(5);
constexpr static const int kKeepAliveProbeCount = 3;

CommonHttpConnection::CommonHttpConnection(
    const ProtocolVersionRange& protocolVersionRange,
    nx::network::aio::AbstractAioThread* aioThread,
    std::shared_ptr<::ec2::ConnectionGuardSharedState> connectionGuardSharedState,
    const transport::ConnectionRequestAttributes& connectionRequestAttributes,
    const std::string& systemId,
    const vms::api::PeerData& localPeer,
    const network::SocketAddress& remotePeerEndpoint,
    const nx::network::http::Request& request)
    :
    CommonHttpConnection(
        protocolVersionRange,
        std::make_unique<::ec2::QnTransactionTransportBase>(
            QnUuid(), //< localSystemId. Not used here
            connectionRequestAttributes.connectionId,
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
            kKeepAliveProbeCount,
            aioThread),
        connectionGuardSharedState,
        systemId,
        remotePeerEndpoint)
{
}

CommonHttpConnection::CommonHttpConnection(
    const ProtocolVersionRange& protocolVersionRange,
    std::unique_ptr<::ec2::QnTransactionTransportBase> connection,
    std::shared_ptr<::ec2::ConnectionGuardSharedState> connectionGuardSharedState,
    const std::string& systemId,
    const network::SocketAddress& remotePeerEndpoint)
    :
    m_protocolVersionRange(protocolVersionRange),
    m_connectionGuardSharedState(connectionGuardSharedState),
    m_baseTransactionTransport(std::move(connection)),
    m_systemId(systemId),
    m_connectionId(m_baseTransactionTransport->connectionGuid()),
    m_connectionOriginatorEndpoint(remotePeerEndpoint),
    m_inactivityTimer(std::make_unique<network::aio::Timer>())
{
    m_baseTransactionTransport->setReceivedTransactionsQueueControlEnabled(false);

    bindToAioThread(m_baseTransactionTransport->getAioThread());
    m_baseTransactionTransport->setState(
        ::ec2::QnTransactionTransportBase::ReadyForStreaming);
    // Ignoring "state changed to Connected" signal.

    QObject::connect(
        m_baseTransactionTransport.get(), &::ec2::QnTransactionTransportBase::gotTransaction,
        this, &CommonHttpConnection::onGotTransaction,
        Qt::DirectConnection);

    QObject::connect(
        m_baseTransactionTransport.get(), &::ec2::QnTransactionTransportBase::stateChanged,
        this, &CommonHttpConnection::onStateChanged,
        Qt::DirectConnection);

    QObject::connect(
        m_baseTransactionTransport.get(), &::ec2::QnTransactionTransportBase::onSomeDataReceivedFromRemotePeer,
        this, &CommonHttpConnection::restartInactivityTimer,
        Qt::DirectConnection);

    if (m_baseTransactionTransport->remotePeerSupportsKeepAlive())
    {
        m_inactivityTimer->start(
            m_baseTransactionTransport->connectionKeepAliveTimeout()
                * m_baseTransactionTransport->keepAliveProbeCount(),
            [this]() { onInactivityTimeout(); });
    }
}

CommonHttpConnection::~CommonHttpConnection()
{
    stopWhileInAioThread();
}

void CommonHttpConnection::bindToAioThread(
    nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_baseTransactionTransport->bindToAioThread(aioThread);
    m_inactivityTimer->bindToAioThread(aioThread);
}

void CommonHttpConnection::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_baseTransactionTransport->stopWhileInAioThread();
    m_inactivityTimer.reset();
}

void CommonHttpConnection::start()
{
    m_baseTransactionTransport->startListening();
}

network::SocketAddress CommonHttpConnection::remotePeerEndpoint() const
{
    return m_connectionOriginatorEndpoint;
}

void CommonHttpConnection::setOnConnectionClosed(ConnectionClosedEventHandler handler)
{
    m_connectionClosedEventHandler = std::move(handler);
}

void CommonHttpConnection::setOnGotTransaction(CommandDataHandler handler)
{
    m_gotTransactionEventHandler = std::move(handler);
}

std::string CommonHttpConnection::connectionGuid() const
{
    return m_baseTransactionTransport->connectionGuid();
}

void CommonHttpConnection::sendTransaction(
    CommandTransportHeader transportHeader,
    const std::shared_ptr<const CommandSerializer>& transactionSerializer)
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

void CommonHttpConnection::closeConnection()
{
    m_baseTransactionTransport->setState(
        ::ec2::QnTransactionTransportBase::Closed);
}

void CommonHttpConnection::receivedTransaction(
    const nx::network::http::HttpHeaders& headers,
    const QnByteArrayConstRef& tranData)
{
    m_baseTransactionTransport->receivedTransaction(headers, tranData);
}

void CommonHttpConnection::setOutgoingConnection(
    std::unique_ptr<network::AbstractCommunicatingSocket> socket)
{
    m_baseTransactionTransport->setOutgoingConnection(std::move(socket));
}

nx::vms::api::PeerData CommonHttpConnection::localPeer() const
{
    return m_baseTransactionTransport->localPeer();
}

nx::vms::api::PeerData CommonHttpConnection::remotePeer() const
{
    return m_baseTransactionTransport->remotePeer();
}

int CommonHttpConnection::remotePeerProtocolVersion() const
{
    return m_baseTransactionTransport->remotePeerProtocolVersion();
}

int CommonHttpConnection::highestProtocolVersionCompatibleWithRemotePeer() const
{
    return m_baseTransactionTransport->remotePeerProtocolVersion() >= m_protocolVersionRange.begin()
        ? m_protocolVersionRange.currentVersion()
        : m_baseTransactionTransport->remotePeerProtocolVersion();
}

void CommonHttpConnection::onGotTransaction(
    Qn::SerializationFormat tranFormat,
    QByteArray data,
    VmsTransportHeader transportHeader)
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

void CommonHttpConnection::forwardTransactionToProcessor(
    Qn::SerializationFormat tranFormat,
    QByteArray data,
    VmsTransportHeader transportHeader)
{
    NX_CRITICAL(isInSelfAioThread());

    // NOTE: Transport sequence MUST be valid for compatibility with VMS <= 3.2.
    // Although, it is not used by this implementation.
    if (m_prevReceivedTransportSequence)
        NX_ASSERT(transportHeader.sequence > *m_prevReceivedTransportSequence);
    m_prevReceivedTransportSequence = transportHeader.sequence;

    if (!m_gotTransactionEventHandler)
        return;

    if (m_closed)
    {
        NX_VERBOSE(this, lm("systemId %1. Received transaction from %2 after connection closure")
            .args(m_systemId, m_connectionOriginatorEndpoint));
        return;
    }

    CommandTransportHeader cdbTransportHeader(m_protocolVersionRange.currentVersion());
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

void CommonHttpConnection::onStateChanged(
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

void CommonHttpConnection::forwardStateChangedEvent(
    ::ec2::QnTransactionTransportBase::State newState)
{
    NX_CRITICAL(isInSelfAioThread());

    if (m_closed)
        return; //< Not reporting multiple close events.

    if (newState == ::ec2::QnTransactionTransportBase::Closed ||
        newState == ::ec2::QnTransactionTransportBase::Error)
    {
        NX_VERBOSE(this,
            lm("systemId %1, connection %2. Reporting connection closure")
                .args(m_systemId, m_connectionId));

        m_closed = true;
        if (m_connectionClosedEventHandler)
            m_connectionClosedEventHandler(SystemError::connectionReset);
    }
}

void CommonHttpConnection::restartInactivityTimer()
{
    NX_CRITICAL(isInSelfAioThread());
    NX_CRITICAL(m_inactivityTimer->isInSelfAioThread());

    m_inactivityTimer->cancelSync();
    m_inactivityTimer->start(
        m_baseTransactionTransport->connectionKeepAliveTimeout()
            * m_baseTransactionTransport->keepAliveProbeCount(),
        std::bind(&CommonHttpConnection::onInactivityTimeout, this));
}

void CommonHttpConnection::onInactivityTimeout()
{
    NX_VERBOSE(this, lm("systemId %1, connection %2. Inactivity timeout triggered")
        .args(m_systemId, m_connectionId));
    m_baseTransactionTransport->setState(::ec2::QnTransactionTransportBase::Error);
}

} // namespace nx::clusterdb::engine::transport
