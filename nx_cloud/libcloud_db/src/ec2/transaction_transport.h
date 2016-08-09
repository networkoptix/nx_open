
#pragma once

#include <nx/network/aio/basic_pollable.h>

#include <transaction/transaction_transport_base.h>


namespace nx {
namespace cdb {
namespace ec2 {

class TransactionTransport
:
    public ::ec2::QnTransactionTransportBase,
    public nx::network::aio::BasicPollable
{
public:
    //!Initializer for incoming connection
    TransactionTransport(
        const nx::String& connectionId,
        const ::ec2::ApiPeerData& remotePeer,
        QSharedPointer<AbstractCommunicatingSocket> socket,
        const nx_http::Request& request,
        const QByteArray& contentEncoding);
    virtual ~TransactionTransport();

    virtual void stopWhileInAioThread() override;

protected:
    virtual void fillAuthInfo(
        const nx_http::AsyncHttpClientPtr& httpClient,
        bool authByKey) override;
};

}   // namespace ec2
}   // namespace cdb
}   // namespace nx
