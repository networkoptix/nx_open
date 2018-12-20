#pragma once

#include "nx/streaming/abstract_data_consumer.h"
#include "network/tcp_connection_processor.h"
#include "network/tcp_listener.h"
#include <core/resource_access/user_access_data.h>

#include <nx/vms/api/data/peer_data.h>
#include <transaction/connection_guard.h>

class QnMediaServerModule;

namespace nx {
namespace p2p {

class MessageBus;
class P2pConnectionProcessorPrivate;

class ConnectionProcessor: public QnTCPConnectionProcessor
{
public:
    ConnectionProcessor(
        std::unique_ptr<nx::network::AbstractStreamSocket> socket,
        QnTcpListener* owner);

    virtual ~ConnectionProcessor();

protected:
    virtual void run() override;

private:
    vms::api::PeerDataEx localPeer() const;
    bool isDisabledPeer(const vms::api::PeerData& remotePeer) const;
    bool isPeerCompatible(const vms::api::PeerDataEx& remotePeer) const;
    Qn::UserAccessData userAccessData(const vms::api::PeerDataEx& remotePeer) const;
    bool canAcceptConnection(const vms::api::PeerDataEx& remotePeer);
    bool tryAcquireConnecting(
        ec2::ConnectionLockGuard& connectionLockGuard, 
        const vms::api::PeerDataEx& remotePeer);
    bool tryAcquireConnected(
        ec2::ConnectionLockGuard& connectionLockGuard,
        const vms::api::PeerDataEx& remotePeer);
private:
    QnMediaServerModule* m_serverModule = nullptr;
};

} // namespace p2p
} // namespace nx
