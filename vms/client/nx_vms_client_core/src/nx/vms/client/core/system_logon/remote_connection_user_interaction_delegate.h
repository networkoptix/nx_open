// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/system_context_aware.h>
#include <nx/vms/client/core/network/remote_connection_user_interaction_delegate.h>
#include <nx/vms/client/core/network/server_certificate_validation_level.h>
#include <nx/vms/client/core/system_logon/certificate_warning.h>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API RemoteConnectionUserInteractionDelegate:
    public AbstractRemoteConnectionUserInteractionDelegate,
    public SystemContextAware
{
    Q_OBJECT
    using base_type = nx::vms::client::core::AbstractRemoteConnectionUserInteractionDelegate;

public:
    using TokenValidator =
        std::function<bool (const nx::network::http::Credentials& credentials)>;
    using AskUserToAcceptCertificates = std::function<bool (
        const QList<TargetCertificateInfo>& certificatesInfo,
        CertificateWarning::Reason warningType)>;
    using ShowCertificateError = std::function<void (
        const TargetCertificateInfo& certificateInfo)>;

    RemoteConnectionUserInteractionDelegate(
        SystemContext* context,
        TokenValidator validateToken,
        AskUserToAcceptCertificates askToAcceptCertificates,
        ShowCertificateError showCertificateError,
        QObject* parent = nullptr);

    virtual ~RemoteConnectionUserInteractionDelegate() override;

    virtual bool acceptNewCertificate(
        const TargetCertificateInfo& certificateInfo) override;

    virtual bool acceptCertificateAfterMismatch(
        const TargetCertificateInfo& certificateInfo) override;

    virtual bool acceptCertificatesOfServersInTargetSystem(
        const QList<TargetCertificateInfo>& certificatesInfo) override;

    virtual bool request2FaValidation(const nx::network::http::Credentials& credentials) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
