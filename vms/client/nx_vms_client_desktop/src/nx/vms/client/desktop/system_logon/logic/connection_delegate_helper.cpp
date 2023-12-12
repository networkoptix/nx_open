// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "connection_delegate_helper.h"

#include <nx/network/ssl/certificate.h>
#include <nx/vms/api/data/module_information.h>
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

    const auto askToAcceptCertificates =
        [mainWindow](
            const QList<core::TargetCertificateInfo>& certificatesInfo,
            core::CertificateWarning::Reason warningType)
        {
            if (!mainWindow)
                return false;

            ServerCertificateWarning warning(certificatesInfo, warningType, mainWindow);
            warning.exec();

            // Dialog uses both standard and custom buttons. Check button role instead of result code.
            return warning.buttonRole(warning.clickedButton()) == QDialogButtonBox::AcceptRole;
        };

    const auto showCertificateError =
        [mainWindow](const core::TargetCertificateInfo& certificateInfo)
        {
            if (!mainWindow)
                return;

            ServerCertificateError msg(certificateInfo, mainWindow);
            msg.exec();
        };

    return std::make_unique<core::RemoteConnectionUserInteractionDelegate>(
        validateToken, askToAcceptCertificates, showCertificateError);
}

} // namespace nx::vms::client::desktop
