#pragma once

#include "message_bus_adapter.h"

#include <nx/utils/thread/mutex.h>

namespace ec2 {

class ThreadsafeMessageBusAdapter: public TransactionMessageBusAdapter
{
    using base_type = TransactionMessageBusAdapter;

public:
    using TransactionMessageBusAdapter::TransactionMessageBusAdapter;

    virtual void start() override;
    virtual void stop() override;

    virtual QSet<QnUuid> directlyConnectedClientPeers() const override;

    virtual QnUuid routeToPeerVia(
        const QnUuid& dstPeer, int* distance, nx::network::SocketAddress* knownPeerAddress) const override;
    virtual int distanceToPeer(const QnUuid& dstPeer) const override;

    virtual void addOutgoingConnectionToPeer(
        const QnUuid& id,
        nx::vms::api::PeerType peerType,
        const nx::utils::Url& url) override;
    virtual void removeOutgoingConnectionFromPeer(const QnUuid& id) override;

    virtual void dropConnections() override;

    virtual QVector<QnTransportConnectionInfo> connectionsInfo() const override;

    virtual void setHandler(ECConnectionNotificationManager* handler) override;
    virtual void removeHandler(ECConnectionNotificationManager* handler) override;

    virtual QnJsonTransactionSerializer* jsonTranSerializer() const override;
    virtual QnUbjsonTransactionSerializer* ubjsonTranSerializer() const override;

    virtual ConnectionGuardSharedState* connectionGuardSharedState() override;

private:
    mutable QnMutex m_mutex;
};

} // namespace ec2
