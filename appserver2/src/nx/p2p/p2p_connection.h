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
        const nx::utils::Url& remotePeerUrl,
        std::unique_ptr<QObject> opaqueObject,
        ConnectionLockGuard connectionLockGuard);
    Connection(
        QnCommonModule* commonModule,
        const ApiPeerDataEx& remotePeer,
        const ApiPeerDataEx& localPeer,
        nx::network::WebSocketPtr webSocket,
        const Qn::UserAccessData& userAccessData,
        std::unique_ptr<QObject> opaqueObject,
        ConnectionLockGuard connectionLockGuard);

    const Qn::UserAccessData& userAccessData() const { return m_userAccessData; }
protected:
    virtual void fillAuthInfo(nx_http::AsyncClient* httpClient, bool authByKey) override;
private:
    const Qn::UserAccessData m_userAccessData = Qn::kSystemAccess;
};

} // namespace p2p
} // namespace nx
