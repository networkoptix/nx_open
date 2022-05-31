// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_certificate_warning.h"

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include <nx/network/ssl/certificate.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/client/desktop/statistics/context_statistics_module.h>
#include <nx/vms/common/html/html.h>
#include <ui/statistics/modules/certificate_statistics_module.h>

#include "server_certificate_viewer.h"

namespace nx::vms::client::desktop {

ServerCertificateWarning::ServerCertificateWarning(
    const nx::vms::api::ModuleInformation& target,
    const nx::network::SocketAddress& primaryAddress,
    const nx::network::ssl::CertificateChain& certificates,
    core::CertificateWarning::Reason reason,
    QWidget* parent)
    :
    base_type(parent)
{
    // Prepare target description text.

    // Prepare data.
    const auto icon =
        [reason]()
        {
            switch (reason)
            {
                case core::CertificateWarning::Reason::unknownServer:
                    return QnMessageBox::Icon::Question;
                case core::CertificateWarning::Reason::invalidCertificate:
                    return QnMessageBox::Icon::Warning;
                case core::CertificateWarning::Reason::serverCertificateChanged:
                    return QnMessageBox::Icon::Question;
                default:
                    NX_ASSERT(false, "Unreachable");
                    return QnMessageBox::Icon::NoIcon;
            }

        }();

    const auto header = core::CertificateWarning::header(reason, target.name);
    const auto details = core::CertificateWarning::details(reason, target, primaryAddress);
    const auto advice = core::CertificateWarning::advice(reason);

    // Load data into UI.
    setIcon(icon);
    setText(header);
    setInformativeText(details);

    // Add this text as a separate label to make a proper spacing.
    auto additionalText = new QLabel(advice);
    additionalText->setWordWrap(true);
    addCustomWidget(additionalText);

    auto layout = findChild<QVBoxLayout*>("verticalLayout");
    if (NX_ASSERT(layout))
        layout->setSpacing(12);

    auto statisticsName =
        [reason](const QString& name)
        {
            switch (reason)
            {
                case core::CertificateWarning::Reason::unknownServer:
                    return "unkn_dialog_" + name;
                case core::CertificateWarning::Reason::invalidCertificate:
                case core::CertificateWarning::Reason::serverCertificateChanged:
                    return "invl_dialog_" + name;
            }
            return QString();
        };
    auto statistics = statisticsModule()->certificates();
    statistics->registerClick(statisticsName("open"));

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
            statistics->registerClick(statisticsName("view_cert"));
        });
    addCustomWidget(link);

    // Create 'Connect' button.
    auto connectButton = addButton(
        tr("Connect Anyway"),
        QDialogButtonBox::AcceptRole,
        Qn::ButtonAccent::Warning);

    if (reason == core::CertificateWarning::Reason::invalidCertificate
        || reason == core::CertificateWarning::Reason::serverCertificateChanged)
    {
        // Create mandatory checkbox for additional safety.
        auto checkbox = new QCheckBox(tr("I trust this server"));
        auto updateButtonState =
            [connectButton, checkbox]
            {
                connectButton->setEnabled(checkbox->isChecked());
            };
        connect(checkbox, &QCheckBox::toggled, this, updateButtonState);

        addCustomWidget(checkbox);
        updateButtonState();
    }

    connect(connectButton, &QPushButton::clicked,
        statistics, [=] { statistics->registerClick(statisticsName("connect")); });

    // Create 'Cancel' button.
    setStandardButtons({QDialogButtonBox::Cancel});
}

void ServerCertificateWarning::showEvent(QShowEvent *event)
{
    // Set focus to 'Cancel' button for some additional safety.
    button(QDialogButtonBox::Cancel)->setFocus();
    base_type::showEvent(event);
}

} // namespace nx::vms::client::desktop
