// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>

#include "../abstract_remote_connection_factory_requests.h"

namespace nx::vms::client::core {

class CertificateVerifier;

namespace detail {

/**
 * Helper class to directly send requests related to the remote connection establishing.
 */
class RemoteConnectionFactoryRequestsManager: public AbstractRemoteConnectionFactoryRequestsManager
{
public:
    /**
     * Certificate validator ownership is not taken. It's lifetime must be controlled by the caller.
     */
    RemoteConnectionFactoryRequestsManager(CertificateVerifier* certificateVerifier);
    virtual ~RemoteConnectionFactoryRequestsManager() override;

    virtual ModuleInformationReply getModuleInformation(ContextPtr context) const override;

    virtual ServersInfoReply getServersInfo(ContextPtr context) const override;

    virtual nx::vms::api::UserModelV1 getUserModel(ContextPtr context) const override;
    virtual nx::vms::api::UserRoleModel getUserRoleModel(ContextPtr context) const override;

    virtual nx::vms::api::LoginUser getUserType(ContextPtr context) const override;
    virtual nx::vms::api::LoginSession createLocalSession(ContextPtr context) const override;
    virtual nx::vms::api::LoginSession createTemporaryLocalSession(
        ContextPtr context) const override;

    virtual nx::vms::api::LoginSession getCurrentSession(ContextPtr context) const override;
    virtual void checkDigestAuthentication(ContextPtr context) const override;

    virtual std::future<Context::CloudTokenInfo> issueCloudToken(
        ContextPtr context,
        const QString& cloudSystemId) const override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace detail
} // namespace nx::vms::client::core
