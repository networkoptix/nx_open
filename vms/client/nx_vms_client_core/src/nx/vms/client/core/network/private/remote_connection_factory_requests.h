// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <api/model/ping_reply.h>
#include <nx/cloud/db/api/oauth_data.h>
#include <nx/network/ssl/helpers.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/api/data/login.h>
#include <nx/vms/api/data/module_information.h>

#include "../remote_connection_factory_context.h"

namespace nx::cloud::db::api { class Connection; }

namespace nx::vms::client::core {

class CertificateVerifier;

namespace detail {

/**
 * Helper class to directly send requests related to the remote connection establishing.
 */
class RemoteConnectionFactoryRequestsManager
{
    using Context = RemoteConnectionFactoryContext;
    using ContextPtr = std::shared_ptr<Context>;

public:
    struct ServerCertificatesInfo
    {
        QnUuid serverId;

        // A stub struct used to show certificate warning dialog. Will be removed later.
        nx::vms::api::ModuleInformation serverInfo;

        std::optional<std::string> certificate;
        std::optional<std::string> userProvidedCertificate;
    };

public:
    /**
     * Certificate validator ownership is not taken. It's lifetime must be controlled by the caller.
     */
    RemoteConnectionFactoryRequestsManager(CertificateVerifier* certificateVerifier);
    virtual ~RemoteConnectionFactoryRequestsManager();

    void fillModuleInformationAndCertificate(ContextPtr context);
    nx::vms::api::LoginUser getUserType(ContextPtr context);
    nx::vms::api::LoginSession createLocalSession(ContextPtr context);
    nx::vms::api::LoginSession getCurrentSession(ContextPtr context);
    void checkDigestAuthentication(ContextPtr context);
    nx::cloud::db::api::IssueTokenResponse issueCloudToken(
        ContextPtr context,
        nx::cloud::db::api::Connection* cloudConnection);

    ServerCertificatesInfo targetServerCertificates(ContextPtr context);
    std::vector<ServerCertificatesInfo> pullRestCertificates(ContextPtr context);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace detail
} // namespace nx::vms::client::core
