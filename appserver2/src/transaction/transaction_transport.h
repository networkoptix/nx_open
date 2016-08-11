
#pragma once

#include "transaction_transport_base.h"


namespace ec2 {

class QnTransactionTransport
:
    public QnTransactionTransportBase
{
public:
    /** Initializer for incoming connection */
    QnTransactionTransport(
        const QnUuid& connectionGuid,
        const ApiPeerData& localPeer,
        const ApiPeerData& remotePeer,
        QSharedPointer<AbstractStreamSocket> socket,
        ConnectionType::Type connectionType,
        const nx_http::Request& request,
        const QByteArray& contentEncoding,
        const Qn::UserAccessData &userAccessData);
    /** Initializer for outgoing connection */
    QnTransactionTransport(const ApiPeerData& localPeer);

protected:
    virtual void fillAuthInfo(
        const nx_http::AsyncHttpClientPtr& httpClient,
        bool authByKey) override;
};

}   // namespace ec2
