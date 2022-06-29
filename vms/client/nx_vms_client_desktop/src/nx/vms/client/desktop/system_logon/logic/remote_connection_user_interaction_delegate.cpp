// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "remote_connection_user_interaction_delegate.h"

#include <client/client_settings.h>
#include <client_core/client_core_module.h>
#include <nx/network/ssl/certificate.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_session.h>
#include <nx/vms/client/desktop/system_logon/ui/oauth_login_dialog.h>
#include <ui/dialogs/common/message_box.h>

#include "../ui/server_certificate_error.h"

namespace nx::vms::client::desktop {

using ServerCertificateValidationLevel = nx::vms::client::core::ServerCertificateValidationLevel;

RemoteConnectionUserInteractionDelegate::RemoteConnectionUserInteractionDelegate(
    QWidget* parentWidget,
    QObject* parent)
    :
    base_type(parent),
    m_parentWidget(parentWidget)
{
}

bool RemoteConnectionUserInteractionDelegate::acceptNewCertificate(
    const nx::vms::api::ModuleInformation& target,
    const nx::network::SocketAddress& primaryAddress,
    const nx::network::ssl::CertificateChain& chain)
{
    switch (qnSettings->certificateValidationLevel())
    {
        case ServerCertificateValidationLevel::disabled:
            return true;

        case ServerCertificateValidationLevel::recommended:
            return askUserToAcceptCertificate(
                target,
                primaryAddress,
                chain,
                ServerCertificateWarning::Reason::unknownServer);

        case ServerCertificateValidationLevel::strict:
            showCertificateError(target, primaryAddress, chain);
            return false;

        default:
            NX_ASSERT(false, "Unreachable");
            return false;
    }
}

bool RemoteConnectionUserInteractionDelegate::acceptCertificateAfterMismatch(
    const nx::vms::api::ModuleInformation& target,
    const nx::network::SocketAddress& primaryAddress,
    const nx::network::ssl::CertificateChain& chain)
{
    switch (qnSettings->certificateValidationLevel())
    {
        case ServerCertificateValidationLevel::disabled:
            return true;

        case ServerCertificateValidationLevel::recommended:
            return askUserToAcceptCertificate(
                target,
                primaryAddress,
                chain,
                ServerCertificateWarning::Reason::invalidCertificate);

        case ServerCertificateValidationLevel::strict:
            showCertificateError(target, primaryAddress, chain);
            return false;

        default:
            NX_ASSERT(false, "Unreachable");
            return false;
    }
}

bool RemoteConnectionUserInteractionDelegate::acceptCertificateOfServerInTargetSystem(
    const nx::vms::api::ModuleInformation& target,
    const nx::network::SocketAddress& primaryAddress,
    const nx::network::ssl::CertificateChain& chain)
{
    switch (qnSettings->certificateValidationLevel())
    {
        case ServerCertificateValidationLevel::disabled:
            return true;

        case ServerCertificateValidationLevel::recommended:
            return askUserToAcceptCertificate(
                target,
                primaryAddress,
                chain,
                ServerCertificateWarning::Reason::serverCertificateChanged);

        case ServerCertificateValidationLevel::strict:
            // We don't pin certificates in strict mode, so no warning is shown.
            return true;

        default:
            NX_ASSERT(false, "Unreachable");
            return false;
    }
}

bool RemoteConnectionUserInteractionDelegate::askUserToAcceptCertificate(
    const nx::vms::api::ModuleInformation& target,
    const nx::network::SocketAddress& primaryAddress,
    const nx::network::ssl::CertificateChain& chain,
    ServerCertificateWarning::Reason warningType)
{
    if (!m_parentWidget)
        return false;

    ServerCertificateWarning warning(target, primaryAddress, chain, warningType, m_parentWidget);
    warning.exec();

    // Dialog uses both standard and custom buttons. Check button role instead of result code.
    return warning.buttonRole(warning.clickedButton()) == QDialogButtonBox::AcceptRole;
}

void RemoteConnectionUserInteractionDelegate::showCertificateError(
    const nx::vms::api::ModuleInformation& target,
    const nx::network::SocketAddress& primaryAddress,
    const nx::network::ssl::CertificateChain& chain)
{
    if (!m_parentWidget)
        return;

    if (const auto session = qnClientCoreModule->networkModule()->session();
        session && session->state() == nx::vms::client::core::RemoteSession::State::reconnecting)
    {
        return;
    }

    ServerCertificateError msg(target, primaryAddress, chain, m_parentWidget);
    msg.exec();
}

bool RemoteConnectionUserInteractionDelegate::request2FaValidation(const std::string& token)
{
    if (!m_parentWidget)
        return false;

    return OauthLoginDialog::validateToken(
        m_parentWidget, tr("Connect to System"), token);
}

} // namespace nx::vms::client::desktop
