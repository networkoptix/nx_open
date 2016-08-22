/**********************************************************
* Aug 10, 2016
* a.kolesnikov
***********************************************************/

#include "transaction_transport.h"

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>


namespace nx {
namespace cdb {
namespace ec2 {

constexpr static const std::chrono::seconds kTcpKeepAliveTimeout = std::chrono::seconds(5);
constexpr static const int kKeepAliveProbeCount = 3;
/** holding in queue not more then this transaction count */
constexpr static const int kMaxTransactionsPerIteration = 17;

TransactionTransport::TransactionTransport(
    TransactionLog* const transactionLog,
    const nx::String& systemId,
    const nx::String& connectionId,
    const ::ec2::ApiPeerData& localPeer,
    const ::ec2::ApiPeerData& remotePeer,
    QSharedPointer<AbstractCommunicatingSocket> socket,
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
        socket,
        ::ec2::ConnectionType::incoming,
        request,
        contentEncoding,
        Qn::kSystemAccess,
        kTcpKeepAliveTimeout,
        kKeepAliveProbeCount),
    m_transactionLogReader(std::make_unique<TransactionLogReader>(
        transactionLog,
        systemId,
        remotePeer.dataFormat)),
    m_systemId(systemId),
    m_connectionId(connectionId),
    m_connectionOriginatorEndpoint(socket->getForeignAddress()),
    m_haveToSendSyncDone(false)
{
    using namespace std::placeholders;

    m_transactionLogReader->setOnUbjsonTransactionReady(
        std::bind(&TransactionTransport::addTransportHeaderToUbjsonTransaction, this, _1));
    m_transactionLogReader->setOnJsonTransactionReady(
        std::bind(&TransactionTransport::addTransportHeaderToJsonTransaction, this, _1));

    nx::network::aio::BasicPollable::bindToAioThread(socket->getAioThread());
    m_transactionLogReader->bindToAioThread(socket->getAioThread());
    setState(Connected);
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
    stopWhileInAioThread();
}

void TransactionTransport::bindToAioThread(nx::network::aio::AbstractAioThread* aioThread)
{
    //implementation should be done in ::ec2::QnTransactionTransportBase
    NX_ASSERT(false);
    //nx::network::aio::BasicPollable::bindToAioThread(aioThread);
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

void TransactionTransport::processSyncRequest(
    const TransactionTransportHeader& /*transportHeader*/,
    ::ec2::ApiSyncRequestData data,
    TransactionProcessedHandler handler)
{
    m_tranStateToSynchronizeTo = m_transactionLogReader->getCurrentState();
    m_remotePeerTranState = std::move(data.persistentState);

    //sending sync response
    ::ec2::QnTransaction<::ec2::QnTranStateResponse>
        tranSyncResponse(::ec2::ApiCommand::tranSyncResponse);
    tranSyncResponse.params.result = 0;

    TransactionTransportHeader transportHeader;
    transportHeader.vmsTransportHeader.processedPeers.insert(localPeer().id);
    sendTransaction(std::move(tranSyncResponse), std::move(transportHeader));

    m_haveToSendSyncDone = true;

    //starting transactions delivery
    using namespace std::placeholders;
    m_transactionLogReader->getTransactions(
        m_remotePeerTranState,
        m_tranStateToSynchronizeTo,
        kMaxTransactionsPerIteration,
        std::bind(&TransactionTransport::onTransactionsReadFromLog, this, _1, _2));
}

void TransactionTransport::onTransactionsReadFromLog(
    api::ResultCode resultCode,
    std::vector<nx::Buffer> serializedTransactions)
{
    //TODO handle api::ResultCode::tryLater result code

    if (resultCode != api::ResultCode::ok)
    {
        NX_LOGX(lm("m_transactionLogReader->getTransactions returned %1. "
            "Closing connection %2 from peer (%3; %4)")
            .arg(api::toString(resultCode)).arg(m_connectionId)
            .arg(remotePeer().id).arg(m_connectionOriginatorEndpoint.toString()),
            cl_logDEBUG1);
        setState(Closed);   //closing connection
    }

    //TODO posting transactions to send
}

void TransactionTransport::processSyncResponse(
    const TransactionTransportHeader& transportHeader,
    ::ec2::QnTranStateResponse data,
    TransactionProcessedHandler handler)
{
    //TODO no need to do anything?
}

void TransactionTransport::processSyncDone(
    const TransactionTransportHeader& transportHeader,
    ::ec2::ApiTranSyncDoneData data,
    TransactionProcessedHandler handler)
{
    //TODO no need to do anything?
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
    if (!m_gotTransactionEventHandler)
        return;

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
    if (newState == ::ec2::QnTransactionTransportBase::Closed ||
        newState == ::ec2::QnTransactionTransportBase::Error)
    {
        if (m_connectionClosedEventHandler)
            m_connectionClosedEventHandler(SystemError::connectionReset);
    }
}

void TransactionTransport::addTransportHeaderToUbjsonTransaction(
    nx::Buffer /*objsonTransaction*/)
{
}

void TransactionTransport::addTransportHeaderToJsonTransaction(
    QJsonObject* /*jsonTransaction*/)
{
}

}   // namespace ec2
}   // namespace cdb
}   // namespace nx
