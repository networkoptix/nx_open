#pragma once

#include "nx/streaming/abstract_data_consumer.h"
#include "network/tcp_connection_processor.h"
#include "network/tcp_listener.h"
#include <nx_ec/data/api_peer_data.h>

class P2pConnectionProcessorPrivate;

namespace nx {
namespace p2p {
class P2pMessageBus;
}
}

class P2pConnectionProcessor : public QnTCPConnectionProcessor
{
public:
    const static QString kUrlPath;

    P2pConnectionProcessor(
        nx::p2p::P2pMessageBus* messageBus,
        QSharedPointer<AbstractStreamSocket> socket,
        QnTcpListener* owner);

    virtual ~P2pConnectionProcessor();
    virtual bool isTakeSockOwnership() const override { return true; }
protected:
    virtual void run() override;
private:
    QByteArray responseBody(Qn::SerializationFormat dataFormat);
    bool isDisabledPeer(const ec2::ApiPeerData& remotePeer) const;
private:
    Q_DECLARE_PRIVATE(P2pConnectionProcessor);
};
