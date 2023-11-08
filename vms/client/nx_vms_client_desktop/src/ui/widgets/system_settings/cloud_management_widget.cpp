// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_management_widget.h"
#include "ui_cloud_management_widget.h"

#include <QtGui/QDesktopServices>

#include <api/server_rest_connection.h>
#include <common/common_module.h>
#include <core/resource/user_resource.h>
#include <nx/branding.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/core/common/utils/cloud_url_helper.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_logon/logic/connect_to_cloud_tool.h>
#include <nx/vms/client/desktop/system_logon/logic/fresh_session_token_helper.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/system_settings.h>
#include <ui/common/palette.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/workbench/workbench_context.h>

using namespace nx::vms::client;
using namespace nx::vms::client::desktop;
using namespace nx::vms::common;

namespace {

const int kAccountFontPixelSize = 24;

core::CloudUrlHelper urlHelper()
{
    using nx::vms::utils::SystemUri;
    return core::CloudUrlHelper(
        SystemUri::ReferralSource::DesktopClient,
        SystemUri::ReferralContext::SettingsDialog);
}

} // namespace

QnCloudManagementWidget::QnCloudManagementWidget(QWidget *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::CloudManagementWidget)
{
    ui->setupUi(this);

    setHelpTopic(this, HelpTopic::Id::SystemSettings_Cloud);

    const QColor nxColor(qApp->palette().color(QPalette::Normal, QPalette::BrightText));
    QFont accountFont(ui->accountLabel->font());
    accountFont.setPixelSize(kAccountFontPixelSize);
    ui->accountLabel->setFont(accountFont);
    for (auto label: { ui->accountLabel, ui->promo1TextLabel, ui->promo2TextLabel, ui->promo3TextLabel })
        setPaletteColor(label, QPalette::WindowText, nxColor);

    ui->arrow1Label->setPixmap(qnSkin->pixmap("promo/cloud_tab_arrow.png"));
    ui->arrow2Label->setPixmap(ui->arrow1Label->pixmap());

    ui->promo1Label->setPixmap(qnSkin->pixmap("promo/cloud_tab_promo_1.png"));
    ui->promo2Label->setPixmap(qnSkin->pixmap("promo/cloud_tab_promo_2.png"));
    ui->promo3Label->setPixmap(qnSkin->pixmap("promo/cloud_tab_promo_3.png"));

    ui->unlinkButton->setText(tr("Disconnect System from %1",
        "%1 is the cloud name (like Nx Cloud)").arg(nx::branding::cloudName()));
    ui->goToCloudButton->setText(tr("Open %1 Portal",
        "%1 is the cloud name (like Nx Cloud)").arg(nx::branding::cloudName()));

    ui->linkButton->setText(tr("Connect System to %1...",
        "%1 is the cloud name (like Nx Cloud)").arg(nx::branding::cloudName()));

    ui->promo1TextLabel->setText(tr("Create %1\naccount",
        "%1 is the cloud name (like Nx Cloud)").arg(nx::branding::cloudName()));
    ui->promo2TextLabel->setText(tr("Connect System\nto %1",
        "%1 is the cloud name (like Nx Cloud)").arg(nx::branding::cloudName()));
    ui->promo3TextLabel->setText(tr("Connect to your Systems\nfrom anywhere with any\ndevices"));

    ui->learnMoreLabel->setText(
        nx::vms::common::html::link(
            tr("Learn more about %1", "%1 is the cloud name (like Nx Cloud)").arg(
                nx::branding::cloudName()),
            urlHelper().aboutUrl()));

    connect(ui->goToCloudButton, &QPushButton::clicked, this,
        [this]
        {
            QDesktopServices::openUrl(urlHelper().mainUrl());
        });

    connect(
        ui->unlinkButton,
        &QPushButton::clicked,
        this,
        &QnCloudManagementWidget::disconnectFromCloud);

    connect(
        ui->linkButton,
        &QPushButton::clicked,
        this,
        &QnCloudManagementWidget::connectToCloud);

    connect(globalSettings(), &SystemSettings::cloudSettingsChanged,
        this, &QnCloudManagementWidget::loadDataToUi);
}

QnCloudManagementWidget::~QnCloudManagementWidget()
{
    if (!NX_ASSERT(!isNetworkRequestRunning(), "Requests should already be completed."))
        discardChanges();
}

void QnCloudManagementWidget::loadDataToUi()
{
    const bool linked = !globalSettings()->cloudSystemId().isEmpty();

    if (linked)
    {
        ui->accountLabel->setText(globalSettings()->cloudAccountName());
        ui->stackedWidget->setCurrentWidget(ui->linkedPage);
    }
    else
    {
        ui->stackedWidget->setCurrentWidget(ui->notLinkedPage);
    }

    const auto isAdministrator = context()->user() && context()->user()->isAdministrator();
    ui->linkButton->setVisible(isAdministrator);
    ui->unlinkButton->setVisible(isAdministrator);
}

bool QnCloudManagementWidget::isNetworkRequestRunning() const
{
    return m_currentRequest != 0;
}

void QnCloudManagementWidget::discardChanges()
{
    if (auto api = connectedServerApi(); api && m_currentRequest > 0)
        api->cancelRequest(m_currentRequest);
    m_currentRequest = 0;
}

void QnCloudManagementWidget::connectToCloud()
{
    NX_ASSERT(!m_connectTool);

    m_connectTool = new ConnectToCloudTool(this, globalSettings());
    connect(
        m_connectTool,
        &ConnectToCloudTool::finished,
        this,
        [this](){ m_connectTool->deleteLater(); });

    m_connectTool->start();
}

void QnCloudManagementWidget::disconnectFromCloud()
{
    if (!NX_ASSERT(m_currentRequest == 0, "Request was already sent"))
        return;

    const bool isAdministrator = context()->user() && context()->user()->isAdministrator();
    if (!NX_ASSERT(isAdministrator, "Button must be unavailable for non-administrator"))
        return;

    if (!confirmCloudDisconnect())
        return;

    const QString title = tr("Disconnect System from %1", "%1 is the cloud name (like Nx Cloud)")
        .arg(nx::branding::cloudName());
    const QString mainText = tr(
        "Enter your account password to disconnect System from %1",
        "%1 is the cloud name (like Nx Cloud)")
            .arg(nx::branding::cloudName());

    auto sessionTokenHelper = FreshSessionTokenHelper::makeHelper(
        this, title, mainText, tr("Disconnect"), FreshSessionTokenHelper::ActionType::unbind);

    auto handler = nx::utils::guarded(
        this,
        [this, isCloudUser = context()->user()->isCloud()](
            bool success, rest::Handle requestId, rest::ErrorOrEmpty reply)
        {
            NX_ASSERT(m_currentRequest == requestId);
            m_currentRequest = 0;
            ui->unlinkButton->hideIndicator();
            ui->unlinkButton->setEnabled(true);

            if (success && std::holds_alternative<rest::Empty>(reply))
            {
                NX_DEBUG(this, "Cloud unbind succeded");
                onDisconnectSuccess();
                return;
            }

            QString errorString;
            if (const auto error = std::get_if<nx::network::rest::Result>(&reply))
            {
                NX_DEBUG(this, "Cloud unbind failed, cloudUser: %1, error: %2, string: %3",
                    isCloudUser, error->error, error->errorString);

                // Sometimes server can break unbind request connection early, since the cloud user
                // is deleted in the process. The request will be automatically retried in
                // that case and will return Unauthorized error.
                if (isCloudUser && (error->error == nx::network::rest::Result::Unauthorized))
                {
                    onDisconnectSuccess();
                    return;
                }

                errorString = std::move(error->errorString);
            }

            QnSessionAwareMessageBox::critical(this,
                tr("Cannot disconnect the System from %1",
                    "%1 is the cloud name (like Nx Cloud)")
                    .arg(nx::branding::cloudName()),
                errorString);
        });

    m_currentRequest = connectedServerApi()->unbindSystemFromCloud(
        sessionTokenHelper,
        /*password*/ QString(),
        std::move(handler),
        this->thread());
    ui->unlinkButton->showIndicator(isNetworkRequestRunning());
    ui->unlinkButton->setEnabled(!isNetworkRequestRunning());
}

void QnCloudManagementWidget::onDisconnectSuccess()
{
    const auto user = context()->user();
    const bool isCloudUser = user ? user->isCloud() : false;

    // Disconnect may follow, but dialog should stay.
    auto messageBox = new QnMessageBox(isCloudUser ? mainWindowWidget() : this);
    messageBox->setIcon(QnMessageBox::Icon::Success);
    messageBox->setText(
        tr("System disconnected from %1", "%1 is the cloud name (like Nx Cloud)")
            .arg(nx::branding::cloudName()));
    messageBox->setStandardButtons(QDialogButtonBox::Ok);
    messageBox->setDefaultButton(QDialogButtonBox::Ok);
    messageBox->setEscapeButton(QDialogButtonBox::Ok);
    connect(messageBox, &QDialog::finished, messageBox, &QObject::deleteLater);
    messageBox->open();

    if (isCloudUser)
        menu()->trigger(ui::action::DisconnectAction, { Qn::ForceRole, true });
}

bool QnCloudManagementWidget::confirmCloudDisconnect()
{
    QnSessionAwareMessageBox messageBox(this);
    messageBox.setIcon(QnMessageBox::Icon::Critical);
    messageBox.setText(tr(
        "You are about to disconnect System from %1",
        "%1 is the cloud name, like Nx Cloud"
    ).arg(nx::branding::cloudName()));

    QString infoText = setWarningStyleHtml(tr(
        "All %1 users will be deleted.", "%1 is the short cloud name (like Cloud)"
    ).arg(nx::branding::shortCloudName()));
    infoText.append("\n");

    if (connection()->connectionInfo().isCloud())
    {
        infoText.append(tr("You will be logged out."))
            .append("\n")
            .append(tr("System will be accessible through local network with"
                " a local administrator account."));
    }
    else
    {
        infoText.append(tr("System will be accessible only through local network."));
    }
    messageBox.setInformativeText(infoText);

    messageBox.addButton(
        tr("Continue"), QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);

    messageBox.setStandardButtons({QDialogButtonBox::Cancel});
    messageBox.button(QDialogButtonBox::Cancel)->setFocus();

    return messageBox.exec() == QDialogButtonBox::AcceptRole;
}
