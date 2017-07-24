#pragma once

#include "nx/streaming/abstract_data_consumer.h"
#include "network/tcp_connection_processor.h"
#include "network/tcp_listener.h"
#include <nx_ec/data/api_peer_data.h>
#include <core/resource_access/user_access_data.h>

namespace nx {
namespace p2p {

class MessageBus;
class P2pConnectionProcessorPrivate;

ec2::ApiPeerDataEx deserializeRemotePeerInfo(const nx_http::Request& request);

class ConnectionProcessor: public QnTCPConnectionProcessor
{
public:
    const static QString kUrlPath;

    ConnectionProcessor(
        QSharedPointer<AbstractStreamSocket> socket,
        QnTcpListener* owner);

    virtual ~ConnectionProcessor();

protected:
    virtual void run() override;

private:
    QByteArray responseBody(Qn::SerializationFormat dataFormat);
    bool isDisabledPeer(const ec2::ApiPeerData& remotePeer) const;
    bool isPeerCompatible(const ec2::ApiPeerDataEx& remotePeer) const;
    Qn::UserAccessData userAccessData(const ec2::ApiPeerDataEx& remotePeer) const;
};

} // namespace p2p
} // namespace nx
