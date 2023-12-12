// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_certificate_error.h"

#include <QtWidgets/QLabel>

#include <nx/network/socket_common.h>
#include <nx/vms/client/core/network/remote_connection_user_interaction_delegate.h>
#include <nx/vms/client/core/system_logon/certificate_warning.h>
#include <nx/vms/client/desktop/statistics/context_statistics_module.h>
#include <nx/vms/common/html/html.h>
#include <ui/statistics/modules/certificate_statistics_module.h>

#include "server_certificate_viewer.h"

namespace nx::vms::client::desktop {

ServerCertificateError::ServerCertificateError(
    const core::TargetCertificateInfo& certificateInfo,
    QWidget* parent)
    :
    base_type(parent)
{
    // Fill data.
    setIcon(QnMessageBox::Icon::Critical);
    setText(tr("Failed to connect to server"));
    setInformativeText(core::CertificateWarning::invalidCertificateError());

    auto statistics = statisticsModule()->certificates();

    // Init server certificate `link`
    auto link = new QLabel(common::html::localLink(tr("View certificate")));
    connect(link, &QLabel::linkActivated, this,
        [=, this]
        {
            auto viewer = new ServerCertificateViewer(
                certificateInfo,
                ServerCertificateViewer::Mode::presented,
                this);

            // Show modal.
            viewer->open();
            statistics->registerClick("err_dialog_view_cert");
        });
    addCustomWidget(link);

    // Set MessageBox buttons.
    setStandardButtons({QDialogButtonBox::Ok});
    setDefaultButton(QDialogButtonBox::Ok);

    statistics->registerClick("err_dialog_open");
}

} // namespace nx::vms::client::desktop
