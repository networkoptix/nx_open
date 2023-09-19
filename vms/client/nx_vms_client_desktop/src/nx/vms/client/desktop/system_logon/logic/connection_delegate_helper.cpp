// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "connection_delegate_helper.h"

#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_session.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/desktop/system_logon/ui/oauth_login_dialog.h>
#include <nx/vms/client/desktop/system_logon/ui/server_certificate_error.h>
#include <nx/vms/client/desktop/system_logon/ui/server_certificate_warning.h>

namespace nx::vms::client::desktop {

std::unique_ptr<core::RemoteConnectionUserInteractionDelegate>
    createConnectionUserInteractionDelegate(QPointer<QWidget> mainWindow)
{
    const auto validateToken =
        [mainWindow](const nx::network::http::Credentials& credentials)
        {
            if (!mainWindow)
                return false;

            return OauthLoginDialog::validateToken(
                mainWindow, OauthLoginDialog::tr("Connect to System"), credentials);
        };

    const auto askToAcceptCertificate =
        [mainWindow](const nx::vms::api::ModuleInformation& target,
            const nx::network::SocketAddress& primaryAddress,
            const nx::network::ssl::CertificateChain& chain,
            core::CertificateWarning::Reason warningType)
        {
            if (!mainWindow)
                return false;

            ServerCertificateWarning warning(target, primaryAddress, chain, warningType, mainWindow);
            warning.exec();

            // Dialog uses both standard and custom buttons. Check button role instead of result code.
            return warning.buttonRole(warning.clickedButton()) == QDialogButtonBox::AcceptRole;
        };

    const auto showCertificateError =
        [mainWindow](const nx::vms::api::ModuleInformation& target,
            const nx::network::SocketAddress& primaryAddress,
            const nx::network::ssl::CertificateChain& chain)
        {
            if (!mainWindow)
                return;

            ServerCertificateError msg(target, primaryAddress, chain, mainWindow);
            msg.exec();
        };

    return std::make_unique<core::RemoteConnectionUserInteractionDelegate>(
        validateToken, askToAcceptCertificate, showCertificateError);
}

} // namespace nx::vms::client::desktop
