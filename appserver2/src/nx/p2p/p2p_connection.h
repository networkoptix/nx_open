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
        const vms::api::PeerDataEx& localPeer,
        const nx::utils::Url& remotePeerUrl,
        std::unique_ptr<QObject> opaqueObject,
        ConnectionLockGuard connectionLockGuard);

    Connection(
        QnCommonModule* commonModule,
        const vms::api::PeerDataEx& remotePeer,
        const vms::api::PeerDataEx& localPeer,
        nx::network::WebSocketPtr webSocket,
        const QUrlQuery& requestUrlQuery,
        const Qn::UserAccessData& userAccessData,
        std::unique_ptr<QObject> opaqueObject,
        ConnectionLockGuard connectionLockGuard);

    virtual ~Connection() override;

    const Qn::UserAccessData& userAccessData() const { return m_userAccessData; }
    virtual bool validateRemotePeerData(const vms::api::PeerDataEx& peer) const override;

    /* Check remote peer identityTime and set local value if it greater.
     * @return true if system identity time has been changed.
     */
    static bool checkAndSetSystemIdentityTime(
        const vms::api::PeerDataEx& remotePeer, QnCommonModule* commonModule);

protected:
    virtual void fillAuthInfo(nx::network::http::AsyncClient* httpClient, bool authByKey) override;

private:
    const Qn::UserAccessData m_userAccessData = Qn::kSystemAccess;
};

} // namespace p2p
} // namespace nx
