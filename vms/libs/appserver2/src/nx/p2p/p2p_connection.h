// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/ssl/helpers.h>
#include <common/common_module_aware.h>

#include "p2p_connection_base.h"

namespace nx {
namespace p2p {

class Connection: public ConnectionBase, public /*mixin*/ QnCommonModuleAware
{
public:
    using ValidateRemotePeerFunc =
        nx::utils::MoveOnlyFunc<bool(const vms::api::PeerDataEx&)>;

    Connection(
        nx::network::ssl::AdapterFunc adapterFunc,
        std::optional<nx::network::http::Credentials> credentials,
        QnCommonModule* commonModule,
        const QnUuid& remoteId,
        nx::vms::api::PeerType remotePeerType,
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
    virtual bool fillAuthInfo(nx::network::http::AsyncClient* httpClient, bool authByKey) override;

private:
    const Qn::UserAccessData m_userAccessData = Qn::kSystemAccess;
    ValidateRemotePeerFunc m_validateRemotePeerFunc;
    std::optional<nx::network::http::Credentials> m_credentials;
};

} // namespace p2p
} // namespace nx
