#pragma once

#include "nx/streaming/abstract_data_consumer.h"
#include "network/tcp_connection_processor.h"
#include "network/tcp_listener.h"
#include <nx_ec/data/api_peer_data.h>

namespace ec2
{

class P2pConnectionProcessorPrivate;
class P2pMessageBus;

class P2pConnectionProcessor : public QnTCPConnectionProcessor
{
public:
    P2pConnectionProcessor(
        P2pMessageBus* messageBus,
        QSharedPointer<AbstractStreamSocket> socket,
        QnTcpListener* owner);

    virtual ~P2pConnectionProcessor();
    virtual bool isTakeSockOwnership() const override { return true; }
protected:
    virtual void run() override;
private:
    void addResponseHeaders();
    bool isDisabledPeer(const ApiPeerData& remotePeer) const;
private:
    Q_DECLARE_PRIVATE(P2pConnectionProcessor);
};

}
