// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
        const nx::utils::Url& url,
        std::optional<nx::network::http::Credentials> credentials,
        nx::network::ssl::AdapterFunc adapterFunc) override;
    virtual void removeOutgoingConnectionFromPeer(const QnUuid& id) override;
    virtual void updateOutgoingConnection(
        const QnUuid& id, nx::network::http::Credentials credentials) override;

    virtual void dropConnections() override;

    virtual ConnectionInfos connectionInfos() const override;

    virtual void setHandler(ECConnectionNotificationManager* handler) override;
    virtual void removeHandler(ECConnectionNotificationManager* handler) override;

    virtual QnJsonTransactionSerializer* jsonTranSerializer() const override;
    virtual QnUbjsonTransactionSerializer* ubjsonTranSerializer() const override;

    virtual ConnectionGuardSharedState* connectionGuardSharedState() override;

private:
    mutable nx::Mutex m_mutex;
};

} // namespace ec2
