#pragma once

#include "p2p_connection_base.h"

namespace nx {
namespace p2p {

class Connection: public ConnectionBase, public QnCommonModuleAware
{
public:
    using ValidateRemotePeerFunc =
        nx::utils::MoveOnlyFunc<bool(const vms::api::PeerDataEx&)>;

    Connection(
        QnCommonModule* commonModule,
        const QnUuid& remoteId,
        const vms::api::PeerDataEx& localPeer,
        const nx::utils::Url& remotePeerUrl,
        std::unique_ptr<QObject> opaqueObject,
        ConnectionLockGuard connectionLockGuard,
        ValidateRemotePeerFunc validateRemotePeerFunc);

    Connection(
        QnCommonModule* commonModule,
        const vms::api::PeerDataEx& remotePeer,
        const vms::api::PeerDataEx& localPeer,
        P2pTransportPtr p2pTransport,
        const QUrlQuery& requestUrlQuery,
        const Qn::UserAccessData& userAccessData,
        std::unique_ptr<QObject> opaqueObject,
        ConnectionLockGuard connectionLockGuard);

    void gotPostConnection(std::unique_ptr<nx::network::AbstractStreamServerSocket> socket);

    virtual ~Connection() override;

    const Qn::UserAccessData& userAccessData() const { return m_userAccessData; }
    virtual bool validateRemotePeerData(const vms::api::PeerDataEx& peer) const override;

protected:
    virtual void fillAuthInfo(nx::network::http::AsyncClient* httpClient, bool authByKey) override;

private:
    const Qn::UserAccessData m_userAccessData = Qn::kSystemAccess;
    ValidateRemotePeerFunc m_validateRemotePeerFunc;
};

} // namespace p2p
} // namespace nx
