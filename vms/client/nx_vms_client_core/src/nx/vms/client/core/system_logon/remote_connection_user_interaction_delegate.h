// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/network/remote_connection_user_interaction_delegate.h>
#include <nx/vms/client/core/network/server_certificate_validation_level.h>
#include <nx/vms/client/core/system_logon/certificate_warning.h>

namespace nx::vms::client::core {

class RemoteConnectionUserInteractionDelegate:
    public nx::vms::client::core::AbstractRemoteConnectionUserInteractionDelegate
{
    Q_OBJECT
    using base_type = nx::vms::client::core::AbstractRemoteConnectionUserInteractionDelegate;

public:
    using CertificateValidationLevelGetter =
        std::function<network::server_certificate::ValidationLevel ()>;
    using TokenValidator =
        std::function<bool (const nx::network::http::Credentials& credentials)>;
    using AskUserToAcceptCertificate = std::function<bool (
        const nx::vms::api::ModuleInformation& target,
        const nx::network::SocketAddress& primaryAddress,
        const nx::network::ssl::CertificateChain& chain,
        CertificateWarning::Reason warningType)>;
    using ShowCertificateError = std::function<void (
        const nx::vms::api::ModuleInformation& target,
        const nx::network::SocketAddress& primaryAddress,
        const nx::network::ssl::CertificateChain& chain)>;

    RemoteConnectionUserInteractionDelegate(
        CertificateValidationLevelGetter validationLevel,
        TokenValidator validateToken,
        AskUserToAcceptCertificate askToAcceptCertificate,
        ShowCertificateError showCertificateError,
        QObject* parent = nullptr);

    virtual ~RemoteConnectionUserInteractionDelegate() override;

    virtual bool acceptNewCertificate(
        const nx::vms::api::ModuleInformation& target,
        const nx::network::SocketAddress& primaryAddress,
        const nx::network::ssl::CertificateChain& chain) override;

    virtual bool acceptCertificateAfterMismatch(
        const nx::vms::api::ModuleInformation& target,
        const nx::network::SocketAddress& primaryAddress,
        const nx::network::ssl::CertificateChain& chain) override;

    virtual bool acceptCertificateOfServerInTargetSystem(
        const nx::vms::api::ModuleInformation& target,
        const nx::network::SocketAddress& primaryAddress,
        const nx::network::ssl::CertificateChain& chain) override;

    virtual bool request2FaValidation(const nx::network::http::Credentials& credentials) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
