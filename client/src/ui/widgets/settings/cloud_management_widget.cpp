#include "cloud_management_widget.h"
#include "ui_cloud_management_widget.h"

#include <api/global_settings.h>

#include <helpers/cloud_url_helper.h>

#include <ui/dialogs/link_to_cloud_dialog.h>
#include <ui/dialogs/unlink_from_cloud_dialog.h>
#include <ui/common/palette.h>

#include <utils/common/app_info.h>
#include <utils/common/string.h>

namespace
{
    const int kAccountFontPixelSize = 24;
}

QnCloudManagementWidget::QnCloudManagementWidget(QWidget *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)
    , ui(new Ui::CloudManagementWidget)
{
    ui->setupUi(this);

    const QColor nxColor(qApp->palette().color(QPalette::Normal, QPalette::BrightText));
    QFont accountFont(ui->accountLabel->font());
    accountFont.setPixelSize(kAccountFontPixelSize);
    ui->accountLabel->setFont(accountFont);
    setPaletteColor(ui->accountLabel, QPalette::WindowText, nxColor);

    // TODO: #help Set help topic

    ui->learnMoreLabel->setText(makeHref(tr("Learn more about %1").arg(QnAppInfo::cloudName()),
                                         QnCloudUrlHelper::aboutUrl()));

    connect(ui->goToCloudButton,    &QPushButton::clicked,  action(QnActions::OpenCloudMainUrl),   &QAction::trigger);

    connect(ui->linkButton, &QPushButton::clicked, this, [this]()
    {
        QScopedPointer<QnLinkToCloudDialog> dialog(new QnLinkToCloudDialog(this));
        dialog->exec();
    }
    );


    connect(ui->unlinkButton, &QPushButton::clicked, this, [this]()
    {
        QScopedPointer<QnUnlinkFromCloudDialog> dialog(new QnUnlinkFromCloudDialog(this));
        dialog->exec();
    }
    );

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
}

void QnCloudManagementWidget::applyChanges()
{
}

bool QnCloudManagementWidget::hasChanges() const
{
    return false;
}

