#include "transaction_transport.h"

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace cdb {
namespace ec2 {

constexpr static const std::chrono::seconds kTcpKeepAliveTimeout = std::chrono::seconds(5);
constexpr static const int kKeepAliveProbeCount = 3;
/** Holding in queue not more then this transaction count. */
constexpr static const int kMaxTransactionsPerIteration = 17;

TransactionTransport::TransactionTransport(
    nx::network::aio::AbstractAioThread* aioThread,
    TransactionLog* const transactionLog,
    const nx::String& systemId,
    const nx::String& connectionId,
    const ::ec2::ApiPeerData& localPeer,
    const ::ec2::ApiPeerData& remotePeer,
    const SocketAddress& remotePeerEndpoint,
    const nx_http::Request& request,
    const QByteArray& contentEncoding)
:
    ::ec2::QnTransactionTransportBase(
        QnUuid::fromStringSafe(connectionId),
        ::ec2::ConnectionLockGuard(
            remotePeer.id,
            ::ec2::ConnectionLockGuard::Direction::Incoming),
        localPeer,
        remotePeer,
        ::ec2::ConnectionType::incoming,
        request,
        contentEncoding,
        kTcpKeepAliveTimeout,
        kKeepAliveProbeCount),
    m_transactionLogReader(std::make_unique<TransactionLogReader>(
        transactionLog,
        systemId,
        remotePeer.dataFormat)),
    m_systemId(systemId),
    m_connectionId(connectionId),
    m_connectionOriginatorEndpoint(remotePeerEndpoint),
    m_haveToSendSyncDone(false),
    m_closed(false)
{
    using namespace std::placeholders;

    m_commonTransportHeaderOfRemoteTransaction.connectionId = connectionId;
    m_commonTransportHeaderOfRemoteTransaction.systemId = systemId;
    m_commonTransportHeaderOfRemoteTransaction.endpoint = remotePeerEndpoint;
    m_commonTransportHeaderOfRemoteTransaction.vmsTransportHeader.sender = remotePeer.id;

    //m_transactionLogReader->setOnUbjsonTransactionReady(
    //    std::bind(&TransactionTransport::addTransportHeaderToUbjsonTransaction, this, _1));
    //m_transactionLogReader->setOnJsonTransactionReady(
    //    std::bind(&TransactionTransport::addTransportHeaderToJsonTransaction, this, _1));

    nx::network::aio::BasicPollable::bindToAioThread(aioThread);
    m_transactionLogReader->bindToAioThread(aioThread);
    setState(ReadyForStreaming);
    //ignoring "state changed to Connected" signal

    QObject::connect(
        this, &::ec2::QnTransactionTransportBase::gotTransaction,
        this, &TransactionTransport::onGotTransaction,
        Qt::DirectConnection);
    QObject::connect(
        this, &::ec2::QnTransactionTransportBase::stateChanged,
        this, &TransactionTransport::onStateChanged,
        Qt::DirectConnection);
}

TransactionTransport::~TransactionTransport()
{
    NX_LOGX(QnLog::EC2_TRAN_LOG,
        lm("systemId %1. Closing connection %2")
        .arg(m_systemId).str(m_commonTransportHeaderOfRemoteTransaction),
        cl_logDEBUG1);

    stopWhileInAioThread();
}

void TransactionTransport::bindToAioThread(nx::network::aio::AbstractAioThread* aioThread)
{
    nx::network::aio::BasicPollable::bindToAioThread(aioThread);

    //implementation should be done in ::ec2::QnTransactionTransportBase
    //NX_ASSERT(false);
    //socket->bindToAioThread(aioThread);
    //m_transactionLogReader->bindToAioThread(aioThread);
}

void TransactionTransport::stopWhileInAioThread()
{
    m_transactionLogReader.reset();
}

void TransactionTransport::setOnConnectionClosed(
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    m_connectionClosedEventHandler = std::move(handler);
}

void TransactionTransport::setOnGotTransaction(GotTransactionEventHandler handler)
{
    m_gotTransactionEventHandler = std::move(handler);
}

void TransactionTransport::startOutgoingChannel()
{
    NX_LOGX(QnLog::EC2_TRAN_LOG, 
        lm("Starting outgoing transaction channel to %1")
        .str(m_commonTransportHeaderOfRemoteTransaction),
        cl_logDEBUG1);

    //sending tranSyncRequest
    ::ec2::QnTransaction<::ec2::ApiSyncRequestData>
        requestTran(::ec2::ApiCommand::tranSyncRequest, localPeer().id);
    requestTran.params.persistentState = m_transactionLogReader->getCurrentState();

    TransactionTransportHeader transportHeader;
    transportHeader.vmsTransportHeader.processedPeers << remotePeer().id;
    transportHeader.vmsTransportHeader.processedPeers << localPeer().id;

    sendTransaction(
        std::move(requestTran),
        std::move(transportHeader));
}

void TransactionTransport::processSpecialTransaction(
    const TransactionTransportHeader& /*transportHeader*/,
    ::ec2::QnTransaction<::ec2::ApiSyncRequestData> data,
    TransactionProcessedHandler handler)
{
    setWriteSync(true);

    m_tranStateToSynchronizeTo = m_transactionLogReader->getCurrentState();
    m_remotePeerTranState = std::move(data.params.persistentState);

    //sending sync response
    ::ec2::QnTransaction<::ec2::QnTranStateResponse>
        tranSyncResponse(::ec2::ApiCommand::tranSyncResponse, localPeer().id);
    tranSyncResponse.params.result = 0;

    TransactionTransportHeader transportHeader;
    transportHeader.vmsTransportHeader.processedPeers.insert(localPeer().id);
    sendTransaction(std::move(tranSyncResponse), std::move(transportHeader));

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

void TransactionTransport::sendTransaction(
    TransactionTransportHeader transportHeader,
    const std::shared_ptr<const TransactionWithSerializedPresentation>& transactionSerializer)
{
    transportHeader.vmsTransportHeader.fillSequence(
        localPeer().id,
        localPeer().instanceId);
    auto serializedTransaction = transactionSerializer->serialize(
        remotePeer().dataFormat,
        std::move(transportHeader));

    post(
        [this, 
            serializedTransaction = std::move(serializedTransaction),
            transactionHeader = transactionSerializer->transactionHeader()]()
        {
            // TODO: #ak checking transaction to send queue size
            //if (isReadyToSend(transaction.command) && queue size is too large)
            //    setWriteSync(false);

            if (isReadyToSend(transactionHeader.command))
            {
                addData(std::move(serializedTransaction));
                return;
            }

            NX_LOGX(
                QnLog::EC2_TRAN_LOG,
                lm("Postponing send transaction %1 to %2").str(transactionHeader.command)
                    .str(m_commonTransportHeaderOfRemoteTransaction),
                cl_logDEBUG1);

            //cannot send transaction right now: updating local transaction sequence
            const ::ec2::QnTranStateKey tranStateKey(
                transactionHeader.peerID,
                transactionHeader.persistentInfo.dbID);
            m_tranStateToSynchronizeTo.values[tranStateKey] =
                transactionHeader.persistentInfo.sequence;
            //transaction will be sent later
        });
}

const TransactionTransportHeader& 
    TransactionTransport::commonTransportHeaderOfRemoteTransaction() const
{
    return m_commonTransportHeaderOfRemoteTransaction;
}

void TransactionTransport::fillAuthInfo(
    const nx_http::AsyncHttpClientPtr& /*httpClient*/,
    bool /*authByKey*/)
{
    //this TransactionTransport is never used to setup outgoing connection
    NX_ASSERT(false);
}

void TransactionTransport::onGotTransaction(
    Qn::SerializationFormat tranFormat,
    const QByteArray& data,
    ::ec2::QnTransactionTransportHeader transportHeader)
{
    NX_CRITICAL(isInSelfAioThread());

    if (!m_gotTransactionEventHandler)
        return;

    if (m_closed)
    {
        NX_LOGX(
            lm("systemId %1. Received transaction from %2 after connection closure")
                .arg(m_systemId).str(m_commonTransportHeaderOfRemoteTransaction),
            cl_logDEBUG2);
        return;
    }

    TransactionTransportHeader cdbTransportHeader;
    cdbTransportHeader.endpoint = m_connectionOriginatorEndpoint;
    cdbTransportHeader.systemId = m_systemId;
    cdbTransportHeader.connectionId = m_connectionId;
    cdbTransportHeader.vmsTransportHeader = std::move(transportHeader);
    m_gotTransactionEventHandler(
        tranFormat,
        std::move(data),
        std::move(cdbTransportHeader));
}

void TransactionTransport::onStateChanged(
    ::ec2::QnTransactionTransportBase::State newState)
{
    NX_CRITICAL(isInSelfAioThread());

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
    std::vector<TransactionData> serializedTransactions,
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
                .str(m_commonTransportHeaderOfRemoteTransaction),
            cl_logDEBUG1);
        setState(Closed);   //closing connection
        return;
    }

    NX_LOGX(QnLog::EC2_TRAN_LOG,
        lm("systemId %1. Read %2 transactions from transaction log (result %3). "
           "Posting them to the send queue to %4")
            .arg(m_systemId).arg(serializedTransactions.size()).arg(api::toString(resultCode))
            .str(m_commonTransportHeaderOfRemoteTransaction),
        cl_logDEBUG1);

    // Posting transactions to send
    for (auto& tranData: serializedTransactions)
    {
        TransactionTransportHeader transportHeader;
        transportHeader.systemId = m_systemId;
        transportHeader.vmsTransportHeader.distance = 1;
        transportHeader.vmsTransportHeader.processedPeers.insert(localPeer().id);

        addData(tranData.serializer->serialize(remotePeer().dataFormat, transportHeader));
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
    setWriteSync(true);
    NX_LOGX(QnLog::EC2_TRAN_LOG,
        lm("systemId %1. Enabled output channel to the peer %2")
        .arg(m_systemId).str(m_commonTransportHeaderOfRemoteTransaction),
        cl_logDEBUG1);

    if (m_haveToSendSyncDone)
    {
        m_haveToSendSyncDone = false;

        ::ec2::QnTransaction<::ec2::ApiTranSyncDoneData>
            tranSyncDone(::ec2::ApiCommand::tranSyncDone, localPeer().id);
        tranSyncDone.params.result = 0;

        TransactionTransportHeader transportHeader;
        transportHeader.vmsTransportHeader.processedPeers.insert(localPeer().id);
        sendTransaction(std::move(tranSyncDone), std::move(transportHeader));
    }
}

//void TransactionTransport::addTransportHeaderToUbjsonTransaction(
//    nx::Buffer* const ubjsonTransaction)
//{
//}
//
//void TransactionTransport::addTransportHeaderToJsonTransaction(
//    QJsonObject* /*jsonTransaction*/)
//{
//}

} // namespace ec2
} // namespace cdb
} // namespace nx
