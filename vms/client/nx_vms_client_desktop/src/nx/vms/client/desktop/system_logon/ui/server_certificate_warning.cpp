// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_certificate_warning.h"

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include <nx/network/ssl/certificate.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/help/help_handler.h>
#include <nx/vms/client/desktop/statistics/context_statistics_module.h>
#include <nx/vms/common/html/html.h>
#include <ui/statistics/modules/certificate_statistics_module.h>

#include "server_certificate_viewer.h"

namespace {

const QString kCertificateLink = "#certificate";
const QString kHelpLink = "#help";

} // namespace

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

    const auto header = core::CertificateWarning::header(reason, target);
    const auto details = core::CertificateWarning::details(reason, target, primaryAddress);
    const auto advice = core::CertificateWarning::advice(reason);

    // Load data into UI.
    setIcon(icon);
    setText(header);
    setInformativeText(details);

    if (!advice.isEmpty())
    {
        auto additionalText = new QLabel(advice);
        additionalText->setWordWrap(true);

        auto palette = additionalText->palette();
        palette.setColor(QPalette::WindowText, core::colorTheme()->color("dark13"));
        additionalText->setPalette(palette);

        addCustomWidget(additionalText);
    }

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

    // Init links.
    connect(this , &QnMessageBox::linkActivated, this,
        [=, this](const QString& link)
        {
            if (link == kCertificateLink)
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
            }
            else if (link == kHelpLink)
            {
                HelpHandler::openHelpTopic(HelpTopic::Id::CertificateValidation);
            }
        });

    bool dialogIsWarning = (reason == core::CertificateWarning::Reason::invalidCertificate
        || reason == core::CertificateWarning::Reason::serverCertificateChanged);

    // Create 'Connect' button.
    auto connectButton = addButton(
        dialogIsWarning ? tr("Connect Anyway") : tr("Continue"),
        QDialogButtonBox::AcceptRole,
        dialogIsWarning ? Qn::ButtonAccent::Warning : Qn::ButtonAccent::Standard);

    if (dialogIsWarning)
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

} // namespace nx::vms::client::desktop
