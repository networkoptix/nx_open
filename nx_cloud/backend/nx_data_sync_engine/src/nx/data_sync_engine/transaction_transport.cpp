#include "transaction_transport.h"

#include <nx/fusion/serialization/lexical.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include "compatible_ec2_protocol_version.h"
#include "command_descriptor.h"

namespace nx {
namespace data_sync_engine {

constexpr static const std::chrono::seconds kTcpKeepAliveTimeout = std::chrono::seconds(5);
constexpr static const int kKeepAliveProbeCount = 3;
/** Holding in queue not more then this transaction count. */
constexpr static const int kMaxTransactionsPerIteration = 17;

TransactionTransport::TransactionTransport(
    const ProtocolVersionRange& protocolVersionRange,
    nx::network::aio::AbstractAioThread* aioThread,
    ::ec2::ConnectionGuardSharedState* const connectionGuardSharedState,
    TransactionLog* const transactionLog,
    const ConnectionRequestAttributes& connectionRequestAttributes,
    const nx::String& systemId,
    const vms::api::PeerData& localPeer,
    const network::SocketAddress& remotePeerEndpoint,
    const nx::network::http::Request& request)
    :
    m_protocolVersionRange(protocolVersionRange),
    m_baseTransactionTransport(std::make_unique<::ec2::QnTransactionTransportBase>(
        QnUuid(), //< localSystemId. Not used here
        QnUuid::fromStringSafe(connectionRequestAttributes.connectionId),
        ::ec2::ConnectionLockGuard(
            localPeer.id,
            connectionGuardSharedState,
            connectionRequestAttributes.remotePeer.id,
            ::ec2::ConnectionLockGuard::Direction::Incoming),
        localPeer,
        connectionRequestAttributes.remotePeer,
        ::ec2::ConnectionType::incoming,
        request,
        connectionRequestAttributes.contentEncoding,
        kTcpKeepAliveTimeout,
        kKeepAliveProbeCount)),
    m_transactionLogReader(std::make_unique<TransactionLogReader>(
        transactionLog,
        systemId,
        connectionRequestAttributes.remotePeer.dataFormat)),
    m_systemId(systemId),
    m_connectionId(connectionRequestAttributes.connectionId),
    m_connectionOriginatorEndpoint(remotePeerEndpoint),
    m_commonTransportHeaderOfRemoteTransaction(protocolVersionRange.currentVersion()),
    m_haveToSendSyncDone(false),
    m_closed(false),
    m_inactivityTimer(std::make_unique<network::aio::Timer>())
{
    using namespace std::placeholders;

    m_commonTransportHeaderOfRemoteTransaction.connectionId =
        connectionRequestAttributes.connectionId;
    m_commonTransportHeaderOfRemoteTransaction.systemId = systemId;
    m_commonTransportHeaderOfRemoteTransaction.endpoint = remotePeerEndpoint;
    m_commonTransportHeaderOfRemoteTransaction.vmsTransportHeader.sender =
        connectionRequestAttributes.remotePeer.id;

    bindToAioThread(aioThread);
    m_baseTransactionTransport->setState(::ec2::QnTransactionTransportBase::ReadyForStreaming);
    //ignoring "state changed to Connected" signal

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
            std::bind(&TransactionTransport::onInactivityTimeout, this));
    }
}

TransactionTransport::~TransactionTransport()
{
    NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this), lm("systemId %1. Closing connection %2")
        .arg(m_systemId).arg(m_commonTransportHeaderOfRemoteTransaction));

    stopWhileInAioThread();
}

void TransactionTransport::bindToAioThread(
    nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_baseTransactionTransport->bindToAioThread(aioThread);
    m_transactionLogReader->bindToAioThread(aioThread);
    m_inactivityTimer->bindToAioThread(aioThread);
}

void TransactionTransport::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_baseTransactionTransport->stopWhileInAioThread();
    m_transactionLogReader.reset();
    m_inactivityTimer.reset();
}

network::SocketAddress TransactionTransport::remoteSocketAddr() const
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

const TransactionTransportHeader&
    TransactionTransport::commonTransportHeaderOfRemoteTransaction() const
{
    return m_commonTransportHeaderOfRemoteTransaction;
}

void TransactionTransport::sendTransaction(
    TransactionTransportHeader transportHeader,
    const std::shared_ptr<const SerializableAbstractTransaction>& transactionSerializer)
{
    transportHeader.vmsTransportHeader.fillSequence(
        m_baseTransactionTransport->localPeer().id,
        m_baseTransactionTransport->localPeer().instanceId);
    auto serializedTransaction = transactionSerializer->serialize(
        m_baseTransactionTransport->remotePeer().dataFormat,
        std::move(transportHeader),
        highestProtocolVersionCompatibleWithRemotePeer());

    post(
        [this,
            serializedTransaction = std::move(serializedTransaction),
            transactionHeader = transactionSerializer->transactionHeader()]()
        {
            // TODO: #ak checking transaction to send queue size
            //if (isReadyToSend(transaction.command) && queue size is too large)
            //    setWriteSync(false);

            if (m_baseTransactionTransport->isReadyToSend(transactionHeader.command))
            {
                m_baseTransactionTransport->addDataToTheSendQueue(std::move(serializedTransaction));
                return;
            }

            NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this), lm("Postponing send transaction %1 to %2").arg(transactionHeader.command)
                    .arg(m_commonTransportHeaderOfRemoteTransaction));

            //cannot send transaction right now: updating local transaction sequence
            const vms::api::PersistentIdData tranStateKey(
                transactionHeader.peerID,
                transactionHeader.persistentInfo.dbID);
            m_tranStateToSynchronizeTo.values[tranStateKey] =
                transactionHeader.persistentInfo.sequence;
            //transaction will be sent later
        });
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

void TransactionTransport::startOutgoingChannel()
{
    NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this), lm("Starting outgoing transaction channel to %1")
        .arg(m_commonTransportHeaderOfRemoteTransaction));

    // Sending tranSyncRequest.
    auto requestTran = command::make<command::TranSyncRequest>(
        m_baseTransactionTransport->localPeer().id);
    requestTran.params.persistentState = m_transactionLogReader->getCurrentState();

    TransactionTransportHeader transportHeader(m_protocolVersionRange.currentVersion());
    transportHeader.vmsTransportHeader.processedPeers
        << m_baseTransactionTransport->remotePeer().id;
    transportHeader.vmsTransportHeader.processedPeers
        << m_baseTransactionTransport->localPeer().id;

    sendTransaction<command::TranSyncRequest>(
        std::move(requestTran),
        std::move(transportHeader));
}

void TransactionTransport::processSpecialTransaction(
    const TransactionTransportHeader& /*transportHeader*/,
    Command<vms::api::SyncRequestData> data,
    TransactionProcessedHandler handler)
{
    m_baseTransactionTransport->setWriteSync(true);

    m_tranStateToSynchronizeTo = m_transactionLogReader->getCurrentState();
    m_remotePeerTranState = std::move(data.params.persistentState);

    //sending sync response
    auto tranSyncResponse = command::make<command::TranSyncResponse>(
        m_baseTransactionTransport->localPeer().id);
    tranSyncResponse.params.result = 0;

    TransactionTransportHeader transportHeader(m_protocolVersionRange.currentVersion());
    transportHeader.vmsTransportHeader.processedPeers.insert(
        m_baseTransactionTransport->localPeer().id);
    sendTransaction<command::TranSyncResponse>(
        std::move(tranSyncResponse),
        std::move(transportHeader));

    m_haveToSendSyncDone = true;

    //starting transactions delivery
    using namespace std::placeholders;
    m_transactionLogReader->readTransactions(
        m_remotePeerTranState,
        m_tranStateToSynchronizeTo,
        kMaxTransactionsPerIteration,
        std::bind(&TransactionTransport::onTransactionsReadFromLog, this, _1, _2, _3));

    handler(ResultCode::ok);
}

void TransactionTransport::processSpecialTransaction(
    const TransactionTransportHeader& /*transportHeader*/,
    Command<vms::api::TranStateResponse> /*data*/,
    TransactionProcessedHandler /*handler*/)
{
    // TODO: no need to do anything?
    //NX_ASSERT(false);
}

void TransactionTransport::processSpecialTransaction(
    const TransactionTransportHeader& /*transportHeader*/,
    Command<vms::api::TranSyncDoneData> /*data*/,
    TransactionProcessedHandler /*handler*/)
{
    // TODO: no need to do anything?
    //NX_ASSERT(false);
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
            .arg(m_systemId).arg(m_commonTransportHeaderOfRemoteTransaction));
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
        NX_VERBOSE(QnLog::EC2_TRAN_LOG.join(this), lm("systemId %1, connection %2. Reporting connection closure")
                .args(m_systemId, m_connectionId));

        m_closed = true;
        if (m_connectionClosedEventHandler)
            m_connectionClosedEventHandler(SystemError::connectionReset);
    }
}

void TransactionTransport::onTransactionsReadFromLog(
    ResultCode resultCode,
    std::vector<dao::TransactionLogRecord> serializedTransactions,
    vms::api::TranState readedUpTo)
{
    using namespace std::placeholders;
    // TODO: handle api::ResultCode::tryLater result code

    if ((resultCode != ResultCode::ok) && (resultCode != ResultCode::partialContent))
    {
        NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this), lm("systemId %1. Error reading transaction log (%2). "
               "Closing connection to the peer %3")
                .arg(m_systemId).arg(toString(resultCode))
                .arg(m_commonTransportHeaderOfRemoteTransaction));
        m_baseTransactionTransport->setState(::ec2::QnTransactionTransportBase::Closed);   //closing connection
        return;
    }

    NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this), lm("systemId %1. Read %2 transactions from transaction log (result %3). "
           "Posting them to the send queue to %4")
            .arg(m_systemId).arg(serializedTransactions.size()).arg(toString(resultCode))
            .arg(m_commonTransportHeaderOfRemoteTransaction));

    // Posting transactions to send
    for (auto& tranData: serializedTransactions)
    {
        TransactionTransportHeader transportHeader(m_protocolVersionRange.currentVersion());
        transportHeader.systemId = m_systemId;
        transportHeader.vmsTransportHeader.distance = 1;
        transportHeader.vmsTransportHeader.processedPeers.insert(
            m_baseTransactionTransport->localPeer().id);

        m_baseTransactionTransport->addDataToTheSendQueue(
            tranData.serializer->serialize(
                m_baseTransactionTransport->remotePeer().dataFormat,
                transportHeader,
                highestProtocolVersionCompatibleWithRemotePeer()));
    }

    m_remotePeerTranState = readedUpTo;

    if (resultCode == ResultCode::partialContent
        || m_tranStateToSynchronizeTo > m_remotePeerTranState)
    {
        if (resultCode != ResultCode::partialContent)
        {
            // TODO: Printing remote and local states.
        }

        //< Local state could be updated while we were synchronizing remote peer
        // Continuing reading transactions.
        m_transactionLogReader->readTransactions(
            m_remotePeerTranState,
            m_tranStateToSynchronizeTo,
            kMaxTransactionsPerIteration,
            std::bind(&TransactionTransport::onTransactionsReadFromLog, this, _1, _2, _3));
        return;
    }

    // Sending transactions to remote peer is allowed now
    enableOutputChannel();
}

void TransactionTransport::enableOutputChannel()
{
    m_baseTransactionTransport->setWriteSync(true);
    NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this), lm("systemId %1. Enabled output channel to the peer %2")
        .arg(m_systemId).arg(m_commonTransportHeaderOfRemoteTransaction));

    if (m_haveToSendSyncDone)
    {
        m_haveToSendSyncDone = false;


        auto tranSyncDone = command::make<command::TranSyncDone>(
            m_baseTransactionTransport->localPeer().id);
        tranSyncDone.params.result = 0;

        TransactionTransportHeader transportHeader(m_protocolVersionRange.currentVersion());
        transportHeader.vmsTransportHeader.processedPeers.insert(
            m_baseTransactionTransport->localPeer().id);
        sendTransaction<command::TranSyncDone>(
            std::move(tranSyncDone),
            std::move(transportHeader));
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

template<typename CommandDescriptor>
void TransactionTransport::sendTransaction(
    Command<typename CommandDescriptor::Data> transaction,
    TransactionTransportHeader transportHeader)
{
    NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this), lm("Sending transaction %1 to %2").arg(transaction.command)
        .arg(m_commonTransportHeaderOfRemoteTransaction));

    std::shared_ptr<const SerializableAbstractTransaction> transactionSerializer;
    switch (m_baseTransactionTransport->remotePeer().dataFormat)
    {
        case Qn::UbjsonFormat:
        {
            auto serializedTransaction = QnUbjson::serialized(transaction);
            transactionSerializer =
                std::make_unique<UbjsonSerializedTransaction<typename CommandDescriptor::Data>>(
                    std::move(transaction),
                    std::move(serializedTransaction),
                    m_protocolVersionRange.currentVersion());
            break;
        }

        default:
        {
            NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this),
                lm("Cannot send transaction in unsupported format %1 to %2")
                .arg(QnLexical::serialized(m_baseTransactionTransport->remotePeer().dataFormat))
                .arg(m_commonTransportHeaderOfRemoteTransaction));

            // TODO: #ak close connection
            NX_ASSERT(false);
            return;
        }
    }

    sendTransaction(
        std::move(transportHeader),
        transactionSerializer);
}

} // namespace data_sync_engine
} // namespace nx
