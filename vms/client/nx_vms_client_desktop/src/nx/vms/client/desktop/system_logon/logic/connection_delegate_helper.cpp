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
    createConnectionUserInteractionDelegate(std::function<QWidget*()> parentWidget)
{
    const auto validateToken =
        [parentWidget](const nx::network::http::Credentials& credentials)
        {
            auto parent = parentWidget();
            if (!parent)
                return false;

            return OauthLoginDialog::validateToken(
                parent, OauthLoginDialog::tr("Connect to System"), credentials);
        };

    const auto askToAcceptCertificates =
        [parentWidget](
            const QList<core::TargetCertificateInfo>& certificatesInfo,
            core::CertificateWarning::Reason warningType)
        {
            auto parent = parentWidget();
            if (!parent)
                return false;

            ServerCertificateWarning warning(certificatesInfo, warningType, parent);
            warning.exec();

            // Dialog uses both standard and custom buttons. Check button role instead of result code.
            return warning.buttonRole(warning.clickedButton()) == QDialogButtonBox::AcceptRole;
        };

    const auto showCertificateError =
        [parentWidget](const core::TargetCertificateInfo& certificateInfo)
        {
            auto parent = parentWidget();
            if (!parent)
                return;

            ServerCertificateError msg(certificateInfo, parent);
            msg.exec();
        };

    return std::make_unique<core::RemoteConnectionUserInteractionDelegate>(
        validateToken, askToAcceptCertificates, showCertificateError);
}

} // namespace nx::vms::client::desktop
