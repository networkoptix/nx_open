#pragma once

#include "nx/streaming/abstract_data_consumer.h"
#include "network/tcp_connection_processor.h"
#include "network/tcp_listener.h"
#include <core/resource_access/user_access_data.h>

#include <nx/vms/api/data/peer_data.h>

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
private:
    QnMediaServerModule* m_serverModule = nullptr;
};

} // namespace p2p
} // namespace nx
