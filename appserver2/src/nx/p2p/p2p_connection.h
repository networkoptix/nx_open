#pragma once

#include "p2p_connection_base.h"

namespace nx {
namespace p2p {

class Connection: public ConnectionBase, public QnCommonModuleAware
{
public:
    Connection(
        QnCommonModule* commonModule,
        const QnUuid& remoteId,
        const ApiPeerDataEx& localPeer,
        ConnectionLockGuard connectionLockGuard,
        const QUrl& remotePeerUrl,
        std::unique_ptr<QObject> opaqueObject);
    Connection(
        QnCommonModule* commonModule,
        const ApiPeerDataEx& remotePeer,
        const ApiPeerDataEx& localPeer,
        ConnectionLockGuard connectionLockGuard,
        nx::network::WebSocketPtr webSocket,
        const Qn::UserAccessData& userAccessData,
        std::unique_ptr<QObject> opaqueObject);
protected:
    virtual void fillAuthInfo(nx_http::AsyncClient* httpClient, bool authByKey) override;
};

} // namespace p2p
} // namespace nx
