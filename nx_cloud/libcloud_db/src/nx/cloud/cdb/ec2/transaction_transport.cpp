#include "transaction_transport.h"

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include "compatible_ec2_protocol_version.h"

namespace nx {
namespace cdb {
namespace ec2 {

constexpr static const std::chrono::seconds kTcpKeepAliveTimeout = std::chrono::seconds(5);
constexpr static const int kKeepAliveProbeCount = 3;
/** Holding in queue not more then this transaction count. */
constexpr static const int kMaxTransactionsPerIteration = 17;

TransactionTransport::TransactionTransport(
    nx::network::aio::AbstractAioThread* aioThread,
    ::ec2::ConnectionGuardSharedState* const connectionGuardSharedState,
    TransactionLog* const transactionLog,
    const ConnectionRequestAttributes& connectionRequestAttributes,
    const nx::String& systemId,
    const ::ec2::ApiPeerData& localPeer,
    const network::SocketAddress& remotePeerEndpoint,
    const nx::network::http::Request& request)
:
    m_baseTransactionTransport(
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
        kKeepAliveProbeCount),
    m_transactionLogReader(std::make_unique<TransactionLogReader>(
        transactionLog,
        systemId,
        connectionRequestAttributes.remotePeer.dataFormat)),
    m_systemId(systemId),
    m_connectionId(connectionRequestAttributes.connectionId),
    m_connectionOriginatorEndpoint(remotePeerEndpoint),
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
    m_baseTransactionTransport.setState(::ec2::QnTransactionTransportBase::ReadyForStreaming);
    //ignoring "state changed to Connected" signal

    QObject::connect(
        &m_baseTransactionTransport, &::ec2::QnTransactionTransportBase::gotTransaction,
        this, &TransactionTransport::onGotTransaction,
        Qt::DirectConnection);
    QObject::connect(
        &m_baseTransactionTransport, &::ec2::QnTransactionTransportBase::stateChanged,
        this, &TransactionTransport::onStateChanged,
        Qt::DirectConnection);
    QObject::connect(
        &m_baseTransactionTransport, &::ec2::QnTransactionTransportBase::onSomeDataReceivedFromRemotePeer,
        this, &TransactionTransport::restartInactivityTimer,
        Qt::DirectConnection);

    if (m_baseTransactionTransport.remotePeerSupportsKeepAlive())
    {
        m_inactivityTimer->start(
            m_baseTransactionTransport.connectionKeepAliveTimeout()
                * m_baseTransactionTransport.keepAliveProbeCount(),
            std::bind(&TransactionTransport::onInactivityTimeout, this));
    }
}

TransactionTransport::~TransactionTransport()
{
    NX_LOGX(QnLog::EC2_TRAN_LOG,
        lm("systemId %1. Closing connection %2")
        .arg(m_systemId).arg(m_commonTransportHeaderOfRemoteTransaction),
        cl_logDEBUG1);

    stopWhileInAioThread();
}

void TransactionTransport::bindToAioThread(
    nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_baseTransactionTransport.bindToAioThread(aioThread);
    m_transactionLogReader->bindToAioThread(aioThread);
    m_inactivityTimer->bindToAioThread(aioThread);
}

void TransactionTransport::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_baseTransactionTransport.stopWhileInAioThread();
    m_transactionLogReader.reset();
    m_inactivityTimer.reset();
}

network::SocketAddress TransactionTransport::remoteSocketAddr() const
{
    return m_baseTransactionTransport.remoteSocketAddr();
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
    return m_baseTransactionTransport.connectionGuid();
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
        m_baseTransactionTransport.localPeer().id,
        m_baseTransactionTransport.localPeer().instanceId);
    auto serializedTransaction = transactionSerializer->serialize(
        m_baseTransactionTransport.remotePeer().dataFormat,
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

            if (m_baseTransactionTransport.isReadyToSend(transactionHeader.command))
            {
                m_baseTransactionTransport.addDataToTheSendQueue(std::move(serializedTransaction));
                return;
            }

            NX_LOGX(
                QnLog::EC2_TRAN_LOG,
                lm("Postponing send transaction %1 to %2").arg(transactionHeader.command)
                    .arg(m_commonTransportHeaderOfRemoteTransaction),
                cl_logDEBUG1);

            //cannot send transaction right now: updating local transaction sequence
            const ::ec2::ApiPersistentIdData tranStateKey(
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
    m_baseTransactionTransport.receivedTransaction(headers, tranData);
}

void TransactionTransport::setOutgoingConnection(
    QSharedPointer<network::AbstractCommunicatingSocket> socket)
{
    m_baseTransactionTransport.setOutgoingConnection(std::move(socket));
}

void TransactionTransport::startOutgoingChannel()
{
    NX_LOGX(QnLog::EC2_TRAN_LOG,
        lm("Starting outgoing transaction channel to %1")
        .arg(m_commonTransportHeaderOfRemoteTransaction),
        cl_logDEBUG1);

    //sending tranSyncRequest
    ::ec2::QnTransaction<::ec2::ApiSyncRequestData> requestTran(
        ::ec2::ApiCommand::tranSyncRequest,
        m_baseTransactionTransport.localPeer().id);
    requestTran.params.persistentState = m_transactionLogReader->getCurrentState();

    TransactionTransportHeader transportHeader;
    transportHeader.vmsTransportHeader.processedPeers
        << m_baseTransactionTransport.remotePeer().id;
    transportHeader.vmsTransportHeader.processedPeers
        << m_baseTransactionTransport.localPeer().id;

    sendTransaction(
        std::move(requestTran),
        std::move(transportHeader));
}

void TransactionTransport::processSpecialTransaction(
    const TransactionTransportHeader& /*transportHeader*/,
    ::ec2::QnTransaction<::ec2::ApiSyncRequestData> data,
    TransactionProcessedHandler handler)
{
    m_baseTransactionTransport.setWriteSync(true);

    m_tranStateToSynchronizeTo = m_transactionLogReader->getCurrentState();
    m_remotePeerTranState = std::move(data.params.persistentState);

    //sending sync response
    ::ec2::QnTransaction<::ec2::QnTranStateResponse> tranSyncResponse(
        ::ec2::ApiCommand::tranSyncResponse,
        m_baseTransactionTransport.localPeer().id);
    tranSyncResponse.params.result = 0;

    TransactionTransportHeader transportHeader;
    transportHeader.vmsTransportHeader.processedPeers.insert(
        m_baseTransactionTransport.localPeer().id);
    sendTransaction(
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

    handler(api::ResultCode::ok);
}

void TransactionTransport::processSpecialTransaction(
    const TransactionTransportHeader& /*transportHeader*/,
    ::ec2::QnTransaction<::ec2::QnTranStateResponse> /*data*/,
    TransactionProcessedHandler /*handler*/)
{
    // TODO: no need to do anything?
    //NX_ASSERT(false);
}

void TransactionTransport::processSpecialTransaction(
    const TransactionTransportHeader& /*transportHeader*/,
    ::ec2::QnTransaction<::ec2::ApiTranSyncDoneData> /*data*/,
    TransactionProcessedHandler /*handler*/)
{
    // TODO: no need to do anything?
    //NX_ASSERT(false);
}

int TransactionTransport::highestProtocolVersionCompatibleWithRemotePeer() const
{
    return m_baseTransactionTransport.remotePeerProtocolVersion() >= kMinSupportedProtocolVersion
        ? kMaxSupportedProtocolVersion
        : m_baseTransactionTransport.remotePeerProtocolVersion();
}

void TransactionTransport::onGotTransaction(
    Qn::SerializationFormat tranFormat,
    QByteArray data,
    ::ec2::QnTransactionTransportHeader transportHeader)
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
    ::ec2::QnTransactionTransportHeader transportHeader)
{
    NX_CRITICAL(isInSelfAioThread());

    if (!m_gotTransactionEventHandler)
        return;

    if (m_closed)
    {
        NX_LOGX(
            lm("systemId %1. Received transaction from %2 after connection closure")
            .arg(m_systemId).arg(m_commonTransportHeaderOfRemoteTransaction),
            cl_logDEBUG2);
        return;
    }

    TransactionTransportHeader cdbTransportHeader;
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
        m_closed = true;
        if (m_connectionClosedEventHandler)
            m_connectionClosedEventHandler(SystemError::connectionReset);
    }
}

void TransactionTransport::onTransactionsReadFromLog(
    api::ResultCode resultCode,
    std::vector<dao::TransactionLogRecord> serializedTransactions,
    ::ec2::QnTranState readedUpTo)
{
    using namespace std::placeholders;
    // TODO: handle api::ResultCode::tryLater result code

    if ((resultCode != api::ResultCode::ok) && (resultCode != api::ResultCode::partialContent))
    {
        NX_LOGX(QnLog::EC2_TRAN_LOG,
            lm("systemId %1. Error reading transaction log (%2). "
               "Closing connection to the peer %3")
                .arg(m_systemId).arg(api::toString(resultCode))
                .arg(m_commonTransportHeaderOfRemoteTransaction),
            cl_logDEBUG1);
        m_baseTransactionTransport.setState(::ec2::QnTransactionTransportBase::Closed);   //closing connection
        return;
    }

    NX_LOGX(QnLog::EC2_TRAN_LOG,
        lm("systemId %1. Read %2 transactions from transaction log (result %3). "
           "Posting them to the send queue to %4")
            .arg(m_systemId).arg(serializedTransactions.size()).arg(api::toString(resultCode))
            .arg(m_commonTransportHeaderOfRemoteTransaction),
        cl_logDEBUG1);

    // Posting transactions to send
    for (auto& tranData: serializedTransactions)
    {
        TransactionTransportHeader transportHeader;
        transportHeader.systemId = m_systemId;
        transportHeader.vmsTransportHeader.distance = 1;
        transportHeader.vmsTransportHeader.processedPeers.insert(
            m_baseTransactionTransport.localPeer().id);

        m_baseTransactionTransport.addDataToTheSendQueue(
            tranData.serializer->serialize(
                m_baseTransactionTransport.remotePeer().dataFormat,
                transportHeader,
                highestProtocolVersionCompatibleWithRemotePeer()));
    }

    m_remotePeerTranState = readedUpTo;

    if (resultCode == api::ResultCode::partialContent
        || m_tranStateToSynchronizeTo > m_remotePeerTranState)
    {
        if (resultCode != api::ResultCode::partialContent)
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
    m_baseTransactionTransport.setWriteSync(true);
    NX_LOGX(QnLog::EC2_TRAN_LOG,
        lm("systemId %1. Enabled output channel to the peer %2")
        .arg(m_systemId).arg(m_commonTransportHeaderOfRemoteTransaction),
        cl_logDEBUG1);

    if (m_haveToSendSyncDone)
    {
        m_haveToSendSyncDone = false;

        ::ec2::QnTransaction<::ec2::ApiTranSyncDoneData>
            tranSyncDone(::ec2::ApiCommand::tranSyncDone, m_baseTransactionTransport.localPeer().id);
        tranSyncDone.params.result = 0;

        TransactionTransportHeader transportHeader;
        transportHeader.vmsTransportHeader.processedPeers.insert(
            m_baseTransactionTransport.localPeer().id);
        sendTransaction(std::move(tranSyncDone), std::move(transportHeader));
    }
}

void TransactionTransport::restartInactivityTimer()
{
    NX_CRITICAL(isInSelfAioThread());
    NX_CRITICAL(m_inactivityTimer->isInSelfAioThread());

    m_inactivityTimer->cancelSync();
    m_inactivityTimer->start(
        m_baseTransactionTransport.connectionKeepAliveTimeout()
            * m_baseTransactionTransport.keepAliveProbeCount(),
        std::bind(&TransactionTransport::onInactivityTimeout, this));
}

void TransactionTransport::onInactivityTimeout()
{
    m_baseTransactionTransport.setState(::ec2::QnTransactionTransportBase::Error);
}

template<class T>
void TransactionTransport::sendTransaction(
    ::ec2::QnTransaction<T> transaction,
    TransactionTransportHeader transportHeader)
{
    NX_LOGX(
        QnLog::EC2_TRAN_LOG,
        lm("Sending transaction %1 to %2").arg(transaction.command)
        .arg(m_commonTransportHeaderOfRemoteTransaction),
        cl_logDEBUG1);

    std::shared_ptr<const SerializableAbstractTransaction> transactionSerializer;
    switch (m_baseTransactionTransport.remotePeer().dataFormat)
    {
        case Qn::UbjsonFormat:
        {
            auto serializedTransaction = QnUbjson::serialized(transaction);
            transactionSerializer =
                std::make_unique<UbjsonSerializedTransaction<T>>(
                    std::move(transaction),
                    std::move(serializedTransaction),
                    nx_ec::EC2_PROTO_VERSION);
            break;
        }

        default:
        {
            NX_LOGX(QnLog::EC2_TRAN_LOG,
                lm("Cannot send transaction in unsupported format %1 to %2")
                .arg(QnLexical::serialized(m_baseTransactionTransport.remotePeer().dataFormat))
                .arg(m_commonTransportHeaderOfRemoteTransaction),
                cl_logDEBUG1);
            // TODO: #ak close connection
            NX_ASSERT(false);
            return;
        }
    }

    sendTransaction(
        std::move(transportHeader),
        transactionSerializer);
}

} // namespace ec2
} // namespace cdb
} // namespace nx
