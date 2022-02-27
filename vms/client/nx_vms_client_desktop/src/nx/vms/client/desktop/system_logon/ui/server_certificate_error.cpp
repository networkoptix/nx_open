// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_certificate_error.h"

#include <QtWidgets/QLabel>

#include <nx/network/socket_common.h>
#include <nx/network/ssl/certificate.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/common/html/html.h>
#include <ui/statistics/modules/controls_statistics_module.h>
#include <ui/workbench/workbench_context.h>

#include "server_certificate_viewer.h"

namespace nx::vms::client::desktop {

ServerCertificateError::ServerCertificateError(
    const nx::vms::api::ModuleInformation& target,
    const nx::network::SocketAddress& primaryAddress,
    const nx::network::ssl::CertificateChain& certificates,
    QWidget* parent)
    :
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    // Fill data.
    setIcon(QnMessageBox::Icon::Critical);
    setText(tr("Failed to connect to server"));
    setInformativeText(tr("Server certificate is invalid."));

    // Init server certificate `link`
    auto link = new QLabel(common::html::localLink(tr("View certificate")));
    connect(link, &QLabel::linkActivated, this,
        [=, this]
        {
            auto viewer = new ServerCertificateViewer(
                target,
                primaryAddress,
                certificates,
                ServerCertificateViewer::Mode::presented,
                this);

            // Show modal.
            viewer->open();
            context()->statisticsModule()->registerClick("cert_err_dialog_view_cert");
        });
    addCustomWidget(link);

    // Set MessageBox buttons.
    setStandardButtons({QDialogButtonBox::Ok});
    setDefaultButton(QDialogButtonBox::Ok);

    context()->statisticsModule()->registerClick("cert_err_dialog_open");
}

} // namespace nx::vms::client::desktop
