#include "cloud_management_widget.h"
#include "ui_cloud_management_widget.h"

#include <api/global_settings.h>

#include <core/resource/user_resource.h>

#include <helpers/cloud_url_helper.h>

#include <ui/common/aligner.h>
#include <ui/dialogs/link_to_cloud_dialog.h>
#include <ui/dialogs/unlink_from_cloud_dialog.h>
#include <ui/help/help_topics.h>
#include <ui/common/palette.h>
#include <ui/style/skin.h>
#include <ui/widgets/common/input_field.h>
#include <ui/workbench/workbench_context.h>

#include <utils/common/app_info.h>
#include <utils/common/html.h>

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
    for (auto label: { ui->accountLabel, ui->promo1TextLabel, ui->promo2TextLabel, ui->promo3TextLabel })
        setPaletteColor(label, QPalette::WindowText, nxColor);

    ui->promo1Label->setPixmap(qnSkin->pixmap("promo/cloud_tab_promo_1.png"));
    ui->promo2Label->setPixmap(qnSkin->pixmap("promo/cloud_tab_promo_2.png"));
    ui->promo3Label->setPixmap(qnSkin->pixmap("promo/cloud_tab_promo_3.png"));

    // TODO: #help Set help topic

    ui->learnMoreLabel->setText(makeHref(tr("Learn more about %1").arg(QnAppInfo::cloudName()),
                                         QnCloudUrlHelper::aboutUrl()));

    connect(ui->goToCloudButton,     &QPushButton::clicked, action(QnActions::OpenCloudMainUrl),   &QAction::trigger);
    connect(ui->createAccountButton, &QPushButton::clicked, action(QnActions::OpenCloudRegisterUrl),   &QAction::trigger);
    connect(ui->unlinkButton, &QPushButton::clicked, this, &QnCloudManagementWidget::unlinkFromCloud);
    connect(ui->linkButton, &QPushButton::clicked, this,
        [this]()
        {
            QScopedPointer<QnLinkToCloudDialog> dialog(new QnLinkToCloudDialog(this));
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
    if (!context()->user() && !context()->user()->isOwner())
        return;

    QWidget* authorizeWidget = new QWidget();
    auto* layout = new QVBoxLayout(authorizeWidget);

    auto loginField = new QnInputField();
    loginField->setReadOnly(true);
    loginField->setTitle(tr("Login"));
    loginField->setText(qnGlobalSettings->cloudAccountName());
    layout->addWidget(loginField);

    auto passwordField = new QnInputField();
    passwordField->setTitle(tr("Password"));
    passwordField->setEchoMode(QLineEdit::Password);
    layout->addWidget(passwordField);

    QnAligner* aligner = new QnAligner(authorizeWidget);
    aligner->registerTypeAccessor<QnInputField>(QnInputField::createLabelWidthAccessor());
    aligner->addWidget(loginField);
    aligner->addWidget(passwordField);

    //bool loggedAsCloud = context()->user()->isCloud();

    QScopedPointer<QnWorkbenchStateDependentDialog<QnMessageBox> > messageBox(
        new QnWorkbenchStateDependentDialog<QnMessageBox>(this));
    messageBox->setIcon(QnMessageBox::Question);
    messageBox->setWindowTitle(tr("Disconnect from %1").arg(QnAppInfo::cloudName()));
    messageBox->setText(tr("Disconnect system from %1").arg(QnAppInfo::cloudName()));
//        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
//        mainWindow());
/*    messageBox.setDefaultButton(QDialogButtonBox::Ok);*/
    messageBox->setInformativeText(tr("All cloud users and features will be disabled. "
        "Enter password to continue.")); //TODO: #GDM #tr make sure it will be translated fully
    messageBox->addCustomWidget(authorizeWidget, QnMessageBox::Layout::Main);
    messageBox->exec();

//     QScopedPointer<QnLinkToCloudDialog> dialog(new QnLinkToCloudDialog(this));
//     dialog->exec();

}

