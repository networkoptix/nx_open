
#include "transaction_transport.h"


namespace nx {
namespace cdb {
namespace ec2 {

constexpr static const std::chrono::seconds kTcpKeepAliveTimeout = std::chrono::seconds(5);
constexpr static const int kKeepAliveProbeCount = 3;

TransactionTransport::TransactionTransport(
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
        std::move(socket),
        ::ec2::ConnectionType::incoming,
        request,
        contentEncoding,
        Qn::kSystemAccess,
        kTcpKeepAliveTimeout,
        kKeepAliveProbeCount)
{
}

void TransactionTransport::setOnConnectionClosed(
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    //TODO
}

TransactionTransport::~TransactionTransport()
{
    stopWhileInAioThread();
}

void TransactionTransport::stopWhileInAioThread()
{
    //TODO #ak
}

void TransactionTransport::fillAuthInfo(
    const nx_http::AsyncHttpClientPtr& httpClient,
    bool authByKey)
{
    //this TransactionTransport is never used to setup outgoing connection
    NX_ASSERT(false);
}

}   // namespace ec2
}   // namespace cdb
}   // namespace nx
