// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_management_widget.h"
#include "ui_cloud_management_widget.h"

#include <QtGui/QDesktopServices>

#include <api/server_rest_connection.h>
#include <core/resource/user_resource.h>
#include <nx/branding.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/core/common/utils/cloud_url_helper.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/logic/connect_to_cloud_tool.h>
#include <nx/vms/client/desktop/system_logon/logic/fresh_session_token_helper.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/saas/saas_utils.h>
#include <nx/vms/common/system_settings.h>
#include <ui/common/palette.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/workbench/workbench_context.h>

using namespace nx::vms::client;
using namespace nx::vms::client::desktop;
using namespace nx::vms::common;

namespace {

static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kLight1Theme =
    {{QnIcon::Normal, {.primary = "light1"}}};

NX_DECLARE_COLORIZED_ICON(kUserCloudIcon, "20x20/Solid/user_cloud.svg", core::kEmptySubstitutions)
NX_DECLARE_COLORIZED_ICON(kOrganizationIcon, "20x20/Solid/organization.svg", kLight1Theme)
NX_DECLARE_COLORIZED_ICON(kConnectPlaceholder, "Illustrations/240x240/nx_connect_placeholder.svg", core::kEmptySubstitutions)

core::CloudUrlHelper urlHelper()
{
    using nx::vms::utils::SystemUri;
    return core::CloudUrlHelper(
        SystemUri::ReferralSource::DesktopClient,
        SystemUri::ReferralContext::SettingsDialog);
}

QFont accountLabelFont()
{
    static constexpr auto kAccountFontSize = 18;
    static constexpr auto kAccountFontWeight = QFont::Medium;

    QFont font;
    font.setPixelSize(kAccountFontSize);
    font.setWeight(kAccountFontWeight);
    return font;
}

QFont placeholderCaptionFont()
{
    static constexpr auto kPlaceholderCaptionFontSize = 16;
    static constexpr auto kPlaceholderCaptionFontWeight = QFont::Medium;

    QFont font;
    font.setPixelSize(kPlaceholderCaptionFontSize);
    font.setWeight(kPlaceholderCaptionFontWeight);
    return font;
}

QFont placeholderDescriptionFont()
{
    static constexpr auto kPlaceholderDescriptionFontSize = 14;

    QFont font;
    font.setPixelSize(kPlaceholderDescriptionFontSize);
    return font;
}

} // namespace

QnCloudManagementWidget::QnCloudManagementWidget(QWidget *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::CloudManagementWidget)
{
    ui->setupUi(this);

    setHelpTopic(this, HelpTopic::Id::SystemSettings_Cloud);

    ui->accountLabel->setFont(accountLabelFont());
    setPaletteColor(ui->accountLabel, QPalette::WindowText, palette().color(QPalette::BrightText));
    ui->accountLabelLayout->setSpacing(nx::style::Metrics::kInterSpace);

    ui->unlinkButton->setText(tr("Disconnect Site from %1",
        "%1 is the cloud name (like Nx Cloud)").arg(nx::branding::cloudName()));

    ui->goToCloudButton->setText(tr("Open %1 Portal",
        "%1 is the cloud name (like Nx Cloud)").arg(nx::branding::cloudName()));

    ui->linkButton->setText(tr("Connect Site to %1...",
        "%1 is the cloud name (like Nx Cloud)").arg(nx::branding::cloudName()));

    ui->placeholderCaptionLabel->setFont(placeholderCaptionFont());
    ui->placeholderDescriptionLabel->setFont(placeholderDescriptionFont());

    ui->placeholderDescriptionLabel->setText(
        tr("There is currently no connection between your site and %1.",
            "%1 is the cloud name (like Nx Cloud)").arg(nx::branding::cloudName()));

    ui->notLinkedPlaceholderImageLabel->setPixmap(
        qnSkin->icon(kConnectPlaceholder).pixmap(240, 240));

    connect(ui->goToCloudButton, &QPushButton::clicked, this,
        []
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

    connect(systemContext()->saasServiceManager(), &saas::ServiceManager::dataChanged,
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
        const auto saasInitialized = nx::vms::common::saas::saasInitialized(systemContext());
        const auto saasServiceManager = systemContext()->saasServiceManager();

        const auto accountText = saasInitialized
            ? saasServiceManager->data().organization.name
            : globalSettings()->cloudAccountName();

        const auto accountPixmap = saasInitialized
            ? qnSkin->icon(kOrganizationIcon).pixmap(core::kIconSize)
            : qnSkin->icon(kUserCloudIcon).pixmap(core::kIconSize);

        ui->accountLabel->setText(accountText);
        ui->accountLabelIcon->setPixmap(accountPixmap);

        if (accountText.isEmpty())
            ui->accountStackedWidget->setCurrentWidget(ui->accountPreloaderPage);

        // After system binded to an organization it takes few seconds before SaaS report will be
        // retrieved from channel partners service. Until that system isn't recognized as one using
        // SaaS licensing model, also both cloud account name and organization name will be empty.
        // During this transitional state preloader is shown instead of an organization name.
        ui->accountStackedWidget->setCurrentWidget(accountText.isEmpty()
            ? ui->accountPreloaderPage
            : ui->accountLabelPage);

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

    const QString title = tr("Disconnect Site from %1", "%1 is the cloud name (like Nx Cloud)")
        .arg(nx::branding::cloudName());
    const QString mainText = tr(
        "Enter your account password to disconnect Site from %1",
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

            if (success && reply)
            {
                NX_DEBUG(this, "Cloud unbind succeeded");
                onDisconnectSuccess();
                return;
            }

            QString errorString;
            if (!reply)
            {
                auto error = reply.error();
                NX_DEBUG(this, "Cloud unbind failed, cloudUser: %1, error: %2, string: %3",
                    isCloudUser, error.errorId, error.errorString);

                // Sometimes server can break unbind request connection early, since the cloud user
                // is deleted in the process. The request will be automatically retried in
                // that case and will return Unauthorized error.
                if (isCloudUser && (error.errorId == nx::network::rest::ErrorId::unauthorized))
                {
                    onDisconnectSuccess();
                    return;
                }

                errorString = std::move(error.errorString);
            }

            QnSessionAwareMessageBox::critical(this,
                tr("Cannot disconnect the Site from %1",
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
    core::appContext()->cloudStatusWatcher()->updateSystems();

    const auto user = context()->user();
    const bool isCloudUser = user ? user->isCloud() : false;

    // Disconnect may follow, but dialog should stay.
    auto messageBox = new QnMessageBox(isCloudUser ? mainWindowWidget() : this);
    messageBox->setIcon(QnMessageBox::Icon::Success);
    messageBox->setText(
        tr("Site disconnected from %1", "%1 is the cloud name (like Nx Cloud)")
            .arg(nx::branding::cloudName()));
    messageBox->setStandardButtons(QDialogButtonBox::Ok);
    messageBox->setDefaultButton(QDialogButtonBox::Ok);
    messageBox->setEscapeButton(QDialogButtonBox::Ok);
    connect(messageBox, &QDialog::finished, messageBox, &QObject::deleteLater);
    messageBox->open();

    if (isCloudUser)
        menu()->trigger(menu::DisconnectAction, { Qn::ForceRole, true });
}

bool QnCloudManagementWidget::confirmCloudDisconnect()
{
    const auto isSaasInitialized = nx::vms::common::saas::saasInitialized(systemContext());
    const auto isCloudUser = context()->user()->isCloud();

    QnSessionAwareMessageBox messageBox(this);
    messageBox.setIcon(QnMessageBox::Icon::Question);
    messageBox.setText(tr(
        "Disconnect site from %1?",
        "%1 is the cloud name, like Nx Cloud"
        ).arg(nx::branding::cloudName()));

    QStringList infoTextParagraphs;

    if (isSaasInitialized)
    {
        infoTextParagraphs.append(setWarningStyleHtml(
            tr("Recording will stop and all Service Subscriptions will be removed")));
    }

    infoTextParagraphs.append(setWarningStyleHtml(
        tr("All %1 users will be removed from the site",
            "%1 is the short cloud name (like Cloud)")).arg(nx::branding::shortCloudName()));

    if (isCloudUser)
    {
        infoTextParagraphs.append(tr("You will be logged out of the site. "
            "The site will be accessible only via local network"));
    }
    else
    {
        infoTextParagraphs.append(tr("The site will be accessible only via local network"));
    }

    if (isSaasInitialized)
    {
        infoTextParagraphs.append(
            tr("Existing data (site settings and archive) will be preserved"));
        infoTextParagraphs.append(setWarningStyleHtml(tr("This action cannot be undone")));
    }

    for (auto& paragraph: infoTextParagraphs)
        paragraph = nx::vms::common::html::paragraph(paragraph);

    messageBox.setInformativeText(infoTextParagraphs.join(""));

    messageBox.addButton(
        tr("Disconnect"), QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);

    messageBox.setStandardButtons({QDialogButtonBox::Cancel});
    messageBox.button(QDialogButtonBox::Cancel)->setFocus();

    return messageBox.exec() == QDialogButtonBox::AcceptRole;
}
