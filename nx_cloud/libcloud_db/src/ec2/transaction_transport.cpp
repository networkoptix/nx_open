/**********************************************************
* Aug 10, 2016
* a.kolesnikov
***********************************************************/

#include "transaction_transport.h"


namespace nx {
namespace cdb {
namespace ec2 {

constexpr static const std::chrono::seconds kTcpKeepAliveTimeout = std::chrono::seconds(5);
constexpr static const int kKeepAliveProbeCount = 3;

TransactionTransport::TransactionTransport(
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
        localPeer,
        remotePeer,
        socket,
        ::ec2::ConnectionType::incoming,
        request,
        contentEncoding,
        Qn::kSystemAccess,
        kTcpKeepAliveTimeout,
        kKeepAliveProbeCount),
    m_systemId(systemId),
    m_connectionOriginatorEndpoint(socket->getForeignAddress())
{
    bindToAioThread(socket->getAioThread());
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

void TransactionTransport::stopWhileInAioThread()
{
    //TODO #ak
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

}   // namespace ec2
}   // namespace cdb
}   // namespace nx
