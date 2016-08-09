
#include "transaction_transport.h"


namespace nx {
namespace cdb {
namespace ec2 {

static const QnUuid kCdbGuid("{A1D368E4-3D01-4B18-8642-898272D7CDA9}");

TransactionTransport::TransactionTransport(
    const nx::String& connectionId,
    const ::ec2::ApiPeerData& remotePeer,
    QSharedPointer<AbstractCommunicatingSocket> socket,
    const nx_http::Request& request,
    const QByteArray& contentEncoding)
:
    ::ec2::QnTransactionTransportBase(
        QnUuid::fromStringSafe(connectionId),
        ::ec2::ApiPeerData(kCdbGuid, QnUuid::createUuid(), Qn::PT_CloudServer, Qn::JsonFormat),
        remotePeer,
        std::move(socket),
        ::ec2::ConnectionType::outgoing,
        request,
        contentEncoding,
        Qn::kSystemAccess)
{
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
