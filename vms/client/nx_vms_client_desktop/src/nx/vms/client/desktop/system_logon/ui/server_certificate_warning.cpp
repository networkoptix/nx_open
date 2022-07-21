// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_certificate_warning.h"

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include <network/system_helpers.h>
#include <nx/network/socket_common.h>
#include <nx/network/ssl/certificate.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/common/html/html.h>
#include <ui/help/help_handler.h>
#include <ui/statistics/modules/certificate_statistics_module.h>
#include <ui/workbench/workbench_context.h>

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
    Reason reason,
    QWidget* parent)
    :
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    // Prepare data.
    QString header, details, advice;
    bool dialogIsWarning = false;

    switch (reason)
    {
        case Reason::unknownServer:
        {
            dialogIsWarning = false;

            header = tr("Connecting to %1 for the first time?")
                .arg(helpers::getSystemName(target));
            details = tr(
                "Review the %1 to ensure you trust the server you are connecting to.\n"
                "Read this %2 to learn more about certificate validation.",
                "%1 is <certificate details> link, "
                "%2 is <help article> link")
                .arg(
                    common::html::localLink(tr("certificate details"), kCertificateLink),
                    common::html::localLink(tr("help article"), kHelpLink));
            advice = tr("This message may be shown multiple times when connecting to a multi-server system.");

            break;
        }

        case Reason::invalidCertificate:
        case Reason::serverCertificateChanged:
        {
            dialogIsWarning = true;

            header = tr("Cannot verify the identity of %1").arg(target.name);
            details = tr(
                "This might be due to an expired server certificate or someone trying "
                "to impersonate %1 to steal your personal information.\n"
                "You can view %2 or read this %3 to learn more about the current problem.",
                "%1 is the system name, "
                "%2 is <the server's certificate> link, "
                "%3 is <help article> link")
                .arg(
                    helpers::getSystemName(target),
                    common::html::localLink(tr("the server's certificate"), kCertificateLink),
                    common::html::localLink(tr("help article"), kHelpLink));

            break;
        }

        default:
            NX_ASSERT("Unreachable");
    }


    // Load data into UI.
    setIcon(dialogIsWarning ? QnMessageBox::Icon::Warning : QnMessageBox::Icon::Question);
    setText(header);
    setInformativeText(details);

    if (!advice.isEmpty())
    {
        auto additionalText = new QLabel(advice);
        additionalText->setWordWrap(true);

        auto palette = additionalText->palette();
        palette.setColor(QPalette::Foreground, colorTheme()->color("dark13"));
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
                case Reason::unknownServer:
                    return "unkn_dialog_" + name;
                case Reason::invalidCertificate:
                case Reason::serverCertificateChanged:
                    return "invl_dialog_" + name;
            }
            return QString();
        };
    auto statistics = context()->instance<QnCertificateStatisticsModule>();
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
                QnHelpHandler::openHelpTopic(Qn::CertificateValidation_Help);
            }
        });

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
