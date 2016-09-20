#include "cloud_management_widget.h"
#include "ui_cloud_management_widget.h"

#include <api/global_settings.h>

#include <core/resource/user_resource.h>

#include <helpers/cloud_url_helper.h>

#include <ui/dialogs/cloud/connect_to_cloud_dialog.h>
#include <ui/dialogs/cloud/disconnect_from_cloud_dialog.h>
#include <ui/help/help_topics.h>
#include <ui/common/palette.h>
#include <ui/style/skin.h>

#include <ui/workbench/workbench_context.h>

#include <utils/common/app_info.h>
#include <utils/common/html.h>

namespace
{
const int kAccountFontPixelSize = 24;
}

QnCloudManagementWidget::QnCloudManagementWidget(QWidget *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::CloudManagementWidget)
{
    ui->setupUi(this);

    const QColor nxColor(qApp->palette().color(QPalette::Normal, QPalette::BrightText));
    QFont accountFont(ui->accountLabel->font());
    accountFont.setPixelSize(kAccountFontPixelSize);
    ui->accountLabel->setFont(accountFont);
    for (auto label: { ui->accountLabel, ui->promo1TextLabel, ui->promo2TextLabel, ui->promo3TextLabel })
        setPaletteColor(label, QPalette::WindowText, nxColor);

    ui->arrow1Label->setPixmap(qnSkin->pixmap("promo/cloud_tab_arrow.png"));
    ui->arrow2Label->setPixmap(*ui->arrow1Label->pixmap());

    ui->promo1Label->setPixmap(qnSkin->pixmap("promo/cloud_tab_promo_1.png"));
    ui->promo2Label->setPixmap(qnSkin->pixmap("promo/cloud_tab_promo_2.png"));
    ui->promo3Label->setPixmap(qnSkin->pixmap("promo/cloud_tab_promo_3.png"));

    // TODO: #help Set help topic

    ui->unlinkButton->setText(tr("Disconnect System from %1").arg(QnAppInfo::cloudName()));
    ui->goToCloudButton->setText(tr("Open %1 Portal").arg(QnAppInfo::cloudName()));

    ui->createAccountButton->setText(tr("Create %1 Account").arg(QnAppInfo::cloudName()));
    ui->linkButton->setText(tr("Connect System to %1").arg(QnAppInfo::cloudName()));

    ui->promo1TextLabel->setText(tr("1. Create %1\naccount").arg(QnAppInfo::cloudName()));
    ui->promo2TextLabel->setText(tr("2. Connect system\nto %1").arg(QnAppInfo::cloudName()));
    ui->promo3TextLabel->setText(tr("3. Connect to your systems\nfrom anywhere with any\ndevices"));

    using nx::vms::utils::SystemUri;
    QnCloudUrlHelper urlHelper(
        SystemUri::ReferralSource::DesktopClient,
        SystemUri::ReferralContext::SettingsDialog);

    ui->learnMoreLabel->setText(
        makeHref(tr("Learn more about %1").arg(
            QnAppInfo::cloudName()), urlHelper.aboutUrl()));

    connect(ui->goToCloudButton,     &QPushButton::clicked, action(QnActions::OpenCloudMainUrl),   &QAction::trigger);
    connect(ui->createAccountButton, &QPushButton::clicked, action(QnActions::OpenCloudRegisterUrl),   &QAction::trigger);
    connect(ui->unlinkButton, &QPushButton::clicked, this, &QnCloudManagementWidget::unlinkFromCloud);
    connect(ui->linkButton, &QPushButton::clicked, this,
        [this]()
        {
            QScopedPointer<QnConnectToCloudDialog> dialog(new QnConnectToCloudDialog(this));
            dialog->exec();
        });

    connect(qnGlobalSettings, &QnGlobalSettings::cloudSettingsChanged, this, &QnCloudManagementWidget::loadDataToUi);
}

QnCloudManagementWidget::~QnCloudManagementWidget()
{
}

void QnCloudManagementWidget::loadDataToUi()
{
    bool linked = !qnGlobalSettings->cloudSystemID().isEmpty() &&
        !qnGlobalSettings->cloudAuthKey().isEmpty();

    if (linked)
    {
        ui->accountLabel->setText(qnGlobalSettings->cloudAccountName());
        ui->stackedWidget->setCurrentWidget(ui->linkedPage);
    }
    else
    {
        ui->stackedWidget->setCurrentWidget(ui->notLinkedPage);
    }

    auto isOwner = context()->user() && context()->user()->role() == Qn::UserRole::Owner;
    ui->linkButton->setVisible(isOwner);
    ui->unlinkButton->setVisible(isOwner);
}

void QnCloudManagementWidget::applyChanges()
{
}

bool QnCloudManagementWidget::hasChanges() const
{
    return false;
}

void QnCloudManagementWidget::unlinkFromCloud()
{
    auto isOwner = context()->user() && context()->user()->role() == Qn::UserRole::Owner;
    NX_ASSERT(isOwner, "Button must be unavailable for non-owner");

    if (!isOwner)
        return;

    //bool loggedAsCloud = context()->user()->isCloud();

    QScopedPointer<QnDisconnectFromCloudDialog> messageBox(new QnDisconnectFromCloudDialog(this));
    messageBox->exec();
}

