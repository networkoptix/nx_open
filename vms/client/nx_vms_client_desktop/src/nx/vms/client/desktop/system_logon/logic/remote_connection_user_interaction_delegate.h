// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>
#include <QtWidgets/QWidget>

#include <nx/vms/client/core/network/remote_connection_user_interaction_delegate.h>

#include "../ui/server_certificate_warning.h"

namespace nx::vms::client::desktop {

class RemoteConnectionUserInteractionDelegate:
    public nx::vms::client::core::AbstractRemoteConnectionUserInteractionDelegate
{
    using base_type = nx::vms::client::core::AbstractRemoteConnectionUserInteractionDelegate;
    Q_OBJECT

public:
    RemoteConnectionUserInteractionDelegate(QWidget* parentWidget, QObject* parent = nullptr);

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

    virtual bool request2FaValidation(const std::string& token) override;

private:
    bool askUserToAcceptCertificate(
        const nx::vms::api::ModuleInformation& target,
        const nx::network::SocketAddress& primaryAddress,
        const nx::network::ssl::CertificateChain& chain,
        ServerCertificateWarning::Reason warningType);

    void showCertificateError(
        const nx::vms::api::ModuleInformation& target,
        const nx::network::SocketAddress& primaryAddress,
        const nx::network::ssl::CertificateChain& chain);

private:
    QPointer<QWidget> const m_parentWidget;
};

} // namespace nx::vms::client::desktop
