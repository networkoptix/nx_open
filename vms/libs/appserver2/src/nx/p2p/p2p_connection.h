// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/ssl/helpers.h>
#include <nx/vms/common/system_context_aware.h>

#include "p2p_connection_base.h"

namespace nx {
namespace p2p {

class Connection: public ConnectionBase, public /*mixin*/ nx::vms::common::SystemContextAware
{
public:
    using ValidateRemotePeerFunc =
        nx::utils::MoveOnlyFunc<bool(const vms::api::PeerDataEx&)>;

    using UnauthorizedWatcher = std::function<std::vector<nx::utils::Guard>(
        const nx::network::rest::UserAccessData& userAccessData, std::function<void()> handler)>;

    Connection(
        nx::network::ssl::AdapterFunc adapterFunc,
        std::optional<nx::network::http::Credentials> credentials,
        nx::vms::common::SystemContext* systemContext,
        const nx::Uuid& remoteId,
        nx::vms::api::PeerType remotePeerType,
        const vms::api::PeerDataEx& localPeer,
        const nx::utils::Url& remotePeerUrl,
        std::unique_ptr<QObject> opaqueObject,
        ConnectionLockGuard connectionLockGuard,
        ValidateRemotePeerFunc validateRemotePeerFunc);

    Connection(
        nx::vms::common::SystemContext* systemContext,
        const vms::api::PeerDataEx& remotePeer,
        const vms::api::PeerDataEx& localPeer,
        P2pTransportPtr p2pTransport,
        const QUrlQuery& requestUrlQuery,
        const nx::network::rest::UserAccessData& userAccessData,
        const UnauthorizedWatcher& unauthorizedWatcher,
        std::unique_ptr<QObject> opaqueObject,
        ConnectionLockGuard connectionLockGuard);

    void gotPostConnection(std::unique_ptr<nx::network::AbstractStreamServerSocket> socket);

    virtual ~Connection() override;

    const nx::network::rest::UserAccessData& userAccessData() const { return m_userAccessData; }
    virtual bool validateRemotePeerData(const vms::api::PeerDataEx& peer) const override;
    virtual void updateCredentials(nx::network::http::Credentials credentials);

protected:
    virtual bool fillAuthInfo(nx::network::http::AsyncClient* httpClient, bool authByKey) override;

private:
    void watchForUnauthorize();

private:
    nx::network::rest::UserAccessData m_userAccessData = nx::network::rest::kSystemAccess;
    ValidateRemotePeerFunc m_validateRemotePeerFunc;
    std::optional<nx::network::http::Credentials> m_credentials;
    std::vector<nx::utils::Guard> m_scopeGuards;
    UnauthorizedWatcher m_unauthorizedWatcher;
};

} // namespace p2p
} // namespace nx
