// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <api/model/ping_reply.h>
#include <nx/network/ssl/helpers.h>
#include <nx/vms/api/data/login.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/api/data/user_model.h>
#include <nx/vms/api/data/user_role_model.h>

#include "remote_connection_factory_context.h"

namespace nx::vms::client::core {

/**
 * Helper class to directly send requests related to the remote connection establishing.
 */
class AbstractRemoteConnectionFactoryRequestsManager
{
protected:
    using Context = RemoteConnectionFactoryContext;
    using ContextPtr = std::shared_ptr<Context>;

public:
    virtual ~AbstractRemoteConnectionFactoryRequestsManager() {};

    struct ModuleInformationReply
    {
        nx::vms::api::ModuleInformation moduleInformation;
        nx::network::ssl::CertificateChain handshakeCertificateChain;
    };
    virtual ModuleInformationReply getModuleInformation(ContextPtr context) const = 0;

    struct ServersInfoReply
    {
        std::vector<nx::vms::api::ServerInformation> serversInfo;
        nx::network::ssl::CertificateChain handshakeCertificateChain;
    };
    virtual ServersInfoReply getServersInfo(ContextPtr context) const = 0;

    virtual nx::vms::api::UserModelV1 getUserModel(ContextPtr context) const = 0;
    virtual nx::vms::api::UserRoleModel getUserRoleModel(ContextPtr context) const = 0;

    virtual nx::vms::api::LoginUser getUserType(ContextPtr context) const = 0;
    virtual nx::vms::api::LoginSession createLocalSession(ContextPtr context) const = 0;
    virtual nx::vms::api::LoginSession createTemporaryLocalSession(ContextPtr context) const = 0;
    virtual nx::vms::api::LoginSession getCurrentSession(ContextPtr context) const = 0;
    virtual void checkDigestAuthentication(ContextPtr context) const = 0;

    virtual std::future<Context::CloudTokenInfo> issueCloudToken(
        ContextPtr context,
        const QString& cloudSystemId) const = 0;
};

} // namespace nx::vms::client::core
