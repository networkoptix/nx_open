/**********************************************************
* Aug 10, 2016
* a.kolesnikov
***********************************************************/

#pragma once

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/socket_common.h>
#include <nx/utils/move_only_func.h>

#include <common/common_globals.h>
#include <transaction/transaction_transport_base.h>

#include "transaction_transport_header.h"


namespace nx {
namespace cdb {
namespace ec2 {

class TransactionTransport
:
    public ::ec2::QnTransactionTransportBase,
    public nx::network::aio::BasicPollable
{
public:
    typedef nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> ConnectionClosedEventHandler;
    typedef nx::utils::MoveOnlyFunc<void(
        Qn::SerializationFormat,
        const QByteArray&,
        TransactionTransportHeader)> GotTransactionEventHandler;

    //!Initializer for incoming connection
    TransactionTransport(
        const nx::String& systemId,
        const nx::String& connectionId,
        const ::ec2::ApiPeerData& localPeer,
        const ::ec2::ApiPeerData& remotePeer,
        QSharedPointer<AbstractCommunicatingSocket> socket,
        const nx_http::Request& request,
        const QByteArray& contentEncoding);
    virtual ~TransactionTransport();

    virtual void stopWhileInAioThread() override;

    /** Set handler to be executed when tcp connection is closed */
    void setOnConnectionClosed(ConnectionClosedEventHandler handler);
    void setOnGotTransaction(GotTransactionEventHandler handler);

protected:
    virtual void fillAuthInfo(
        const nx_http::AsyncHttpClientPtr& httpClient,
        bool authByKey) override;

private:
    ConnectionClosedEventHandler m_connectionClosedEventHandler;
    GotTransactionEventHandler m_gotTransactionEventHandler;
    const nx::String m_systemId;
    const SocketAddress m_connectionOriginatorEndpoint;

    void onGotTransaction(
        Qn::SerializationFormat tranFormat,
        const QByteArray& data,
        ::ec2::QnTransactionTransportHeader transportHeader);
    void onStateChanged(::ec2::QnTransactionTransportBase::State newState);
};

}   // namespace ec2
}   // namespace cdb
}   // namespace nx
