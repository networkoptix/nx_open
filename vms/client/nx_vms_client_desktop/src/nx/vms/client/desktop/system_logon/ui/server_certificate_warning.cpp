// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_certificate_warning.h"

#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>

#include <nx/network/ssl/certificate.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/client/core/network/remote_connection_user_interaction_delegate.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/help/help_handler.h>
#include <nx/vms/client/desktop/statistics/context_statistics_module.h>
#include <nx/vms/common/html/html.h>
#include <ui/common/palette.h>
#include <ui/statistics/modules/certificate_statistics_module.h>

#include "server_certificate_viewer.h"

namespace {

const QString kCertificateLink = "#certificate";
const QString kHelpLink = "#help";

} // namespace

namespace nx::vms::client::desktop {

ServerCertificateWarning::ServerCertificateWarning(
    const QList<core::TargetCertificateInfo>& certificatesInfo,
    core::CertificateWarning::Reason reason,
    QWidget* parent)
    :
    base_type(parent)
{
    if (!NX_ASSERT(!certificatesInfo.isEmpty()))
        return;

    // Prepare data.
    const auto icon =
        [reason]()
        {
            switch (reason)
            {
                case core::CertificateWarning::Reason::unknownServer:
                    return QnMessageBox::Icon::Question;
                case core::CertificateWarning::Reason::invalidCertificate:
                    return QnMessageBox::Icon::Critical;
                case core::CertificateWarning::Reason::serverCertificateChanged:
                    return QnMessageBox::Icon::Critical;
                default:
                    NX_ASSERT(false, "Unreachable");
                    return QnMessageBox::Icon::NoIcon;
            }

        }();

    const auto header = core::CertificateWarning::header(
        reason, certificatesInfo.first().target, certificatesInfo.size());
    const auto details = core::CertificateWarning::details(reason);
    const auto advice = core::CertificateWarning::advice(reason);

    // Load data into UI.
    setIcon(icon);
    setText(header);
    setInformativeText(details);

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

    auto frame = new QFrame();
    frame->setFrameStyle(QFrame::Panel | QFrame::Plain);
    frame->setLineWidth(1);

    auto palette = frame->palette();
    palette.setBrush(QPalette::Window, core::colorTheme()->color("dark6"));
    palette.setBrush(QPalette::WindowText, core::colorTheme()->color("dark8"));
    frame->setPalette(palette);
    frame->setAutoFillBackground(true);

    auto frameLayout = new QHBoxLayout(frame);
    frameLayout->setContentsMargins(0, 0, 0, 0);

    auto area = new QScrollArea(frame);
    auto list = new QWidget();
    frameLayout->addWidget(area);

    auto createPanel =
        [=](const core::TargetCertificateInfo& certificateInfo)
        {
            auto widget = new QWidget();

            auto layout = new QVBoxLayout(widget);
            layout->setContentsMargins(8, 8, 8, 10);
            layout->setSpacing(4);

            auto serverIconLabel = new QLabel();
            serverIconLabel->setPixmap(
                qnSkin->icon("events/server_20.svg").pixmap(QSize(20, 20)));

            auto serverNameLabel = new QLabel(certificateInfo.target.name);
            setPaletteColor(serverNameLabel,
                QPalette::WindowText, core::colorTheme()->color("light10"));

            auto serverAddressLabel = new QLabel(
                QString::fromStdString(certificateInfo.address.address.toString()));
            setPaletteColor(serverAddressLabel,
                QPalette::WindowText, core::colorTheme()->color("light16"));

            auto certificateDetailsLabel = new QLabel(
                common::html::localLink("Certificate details", kCertificateLink));

            connect(certificateDetailsLabel , &QLabel::linkActivated, this,
                [=](const QString& link)
                {
                    if (link == kCertificateLink)
                    {
                        auto viewer = new ServerCertificateViewer(
                            certificateInfo,
                            ServerCertificateViewer::Mode::presented,
                            this);

                        // Show modal.
                        viewer->open();
                        statistics->registerClick(statisticsName("view_cert"));
                    }
                });

            auto serverTextLayout = new QHBoxLayout();
            serverTextLayout->setSpacing(4);

            serverTextLayout->addWidget(serverIconLabel);
            serverTextLayout->addWidget(serverNameLabel);
            serverTextLayout->addWidget(serverAddressLabel);
            serverTextLayout->addStretch();

            auto certificateDetailsLayout = new QHBoxLayout();
            certificateDetailsLayout->addSpacing(2);
            certificateDetailsLayout->addWidget(certificateDetailsLabel);

            layout->addLayout(serverTextLayout);
            layout->addLayout(certificateDetailsLayout);

            return widget;
        };

    auto createSeparator =
        []()
        {
            auto line = new QFrame();
            line->setFrameStyle(QFrame::HLine | QFrame::Plain);
            line->setLineWidth(1);

            setPaletteColor(line,
                QPalette::Shadow, core::colorTheme()->color("dark8"));

            return line;
        };

    auto vBox = new QVBoxLayout(list);
    vBox->setContentsMargins(0, 0, 0, 0);
    vBox->setSpacing(0);

    int panelHeight = 0, separatorHeight = 0;
    for (int i = 0; i < certificatesInfo.size(); ++i)
    {
        if (i)
        {
            auto separator = createSeparator();
            separatorHeight = separator->sizeHint().height();
            vBox->addWidget(separator);
        }

        auto panel = createPanel(certificatesInfo[i]);
        panelHeight = panel->sizeHint().height();
        vBox->addWidget(panel);
    }

    area->setWidget(list);
    area->setWidgetResizable(true);
    area->setMaximumHeight(4 * panelHeight + 3 * separatorHeight);
    area->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    addCustomWidget(frame);

    auto layout = findChild<QVBoxLayout*>("verticalLayout");
    if (NX_ASSERT(layout))
        layout->setSpacing(12);

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
        auto checkbox = new QCheckBox(tr("I trust these servers", "", certificatesInfo.size()));
        auto updateButtonState =
            [connectButton, checkbox]
            {
                connectButton->setEnabled(checkbox->isChecked());
            };
        connect(checkbox, &QCheckBox::toggled, this, updateButtonState);

        addCustomWidget(checkbox);
        updateButtonState();
    }

    if (!advice.isEmpty())
    {
        auto additionalText = new QLabel(advice);
        additionalText->setWordWrap(true);

        connect(additionalText , &QLabel::linkActivated, this,
            [=](const QString& link)
            {
                if (link == kHelpLink)
                {
                    HelpHandler::openHelpTopic(HelpTopic::Id::CertificateValidation);
                }
            });

        addCustomWidget(additionalText);
    }

    connect(connectButton, &QPushButton::clicked,
        statistics, [=] { statistics->registerClick(statisticsName("connect")); });

    // Create 'Cancel' button.
    setStandardButtons({QDialogButtonBox::Cancel});
}

} // namespace nx::vms::client::desktop
