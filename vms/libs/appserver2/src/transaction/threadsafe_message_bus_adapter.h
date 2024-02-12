// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/thread/mutex.h>

#include "message_bus_adapter.h"

namespace ec2 {

class ThreadsafeMessageBusAdapter: public TransactionMessageBusAdapter
{
    using base_type = TransactionMessageBusAdapter;

public:
    using TransactionMessageBusAdapter::TransactionMessageBusAdapter;

    virtual void start() override;
    virtual void stop() override;

    virtual QSet<nx::Uuid> directlyConnectedClientPeers() const override;

    virtual nx::Uuid routeToPeerVia(
        const nx::Uuid& dstPeer, int* distance, nx::network::SocketAddress* knownPeerAddress) const override;
    virtual int distanceToPeer(const nx::Uuid& dstPeer) const override;

    virtual void addOutgoingConnectionToPeer(
        const nx::Uuid& id,
        nx::vms::api::PeerType peerType,
        const nx::utils::Url& url,
        std::optional<nx::network::http::Credentials> credentials,
        nx::network::ssl::AdapterFunc adapterFunc) override;
    virtual void removeOutgoingConnectionFromPeer(const nx::Uuid& id) override;
    virtual void updateOutgoingConnection(
        const nx::Uuid& id, nx::network::http::Credentials credentials) override;

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
