#include "cloud_management_widget.h"
#include "ui_cloud_management_widget.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource/param.h>
#include <core/resource/user_resource.h>

#include <utils/common/app_info.h>
#include <utils/common/string.h>
#include <helpers/cloud_url_helper.h>

#include <ui/dialogs/link_to_cloud_dialog.h>
#include <ui/dialogs/unlink_from_cloud_dialog.h>
#include <ui/dialogs/setup_wizard_dialog.h>

class QnCloudManagementWidgetPrivate : public QObject
{
    QnCloudManagementWidget *q_ptr;
    Q_DECLARE_PUBLIC(QnCloudManagementWidget)
    Q_DECLARE_TR_FUNCTIONS(QnCloudManagementWidget)

public:
    QnCloudManagementWidgetPrivate(QnCloudManagementWidget *parent);

    void at_linkButton_clicked();
    void at_unlinkButton_clicked();

    void updateUi();
};

QnCloudManagementWidget::QnCloudManagementWidget(QWidget *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)
    , ui(new Ui::CloudManagementWidget)
    , d_ptr(new QnCloudManagementWidgetPrivate(this))
{
    ui->setupUi(this);

    // TODO: #help Set help topic

    ui->learnMoreLabel->setText(makeHref(tr("Learn more about %1").arg(QnAppInfo::cloudName()),
                                         QnCloudUrlHelper::aboutUrl()));

    Q_D(QnCloudManagementWidget);

    connect(ui->linkButton,         &QPushButton::clicked,  d,  &QnCloudManagementWidgetPrivate::at_linkButton_clicked);
    connect(ui->unlinkButton,       &QPushButton::clicked,  d,  &QnCloudManagementWidgetPrivate::at_unlinkButton_clicked);
    connect(ui->goToCloudButton,    &QPushButton::clicked,  action(QnActions::OpenCloudMainUrl),   &QAction::trigger);

    connect(ui->setupWizardButton,  &QPushButton::clicked,  d, [this]()
        {
            QnSetupWizardDialog *dialog = new QnSetupWizardDialog(this);
            dialog->exec();
        }
    );

    if (QnUserResourcePtr admin = qnResPool->getAdministrator())
    {
        connect(admin, &QnResource::resourceChanged, d, &QnCloudManagementWidgetPrivate::updateUi);
        connect(admin, &QnResource::propertyChanged, d, [this](const QnResourcePtr &resource, const QString &key)
        {
            Q_UNUSED(resource)

            if (key == Qn::CLOUD_ACCOUNT_NAME ||
                key == Qn::CLOUD_SYSTEM_ID ||
                key == Qn::CLOUD_SYSTEM_AUTH_KEY)
            {
                Q_D(QnCloudManagementWidget);
                d->updateUi();
            }
        });
    }
}

QnCloudManagementWidget::~QnCloudManagementWidget()
{
}

void QnCloudManagementWidget::loadDataToUi()
{
    Q_D(QnCloudManagementWidget);
    d->updateUi();
}

void QnCloudManagementWidget::applyChanges()
{
}

bool QnCloudManagementWidget::hasChanges() const
{
    return false;
}


QnCloudManagementWidgetPrivate::QnCloudManagementWidgetPrivate(QnCloudManagementWidget *parent)
    : QObject(parent)
    , q_ptr(parent)
{
}

void QnCloudManagementWidgetPrivate::at_linkButton_clicked()
{
    Q_Q(QnCloudManagementWidget);
    QScopedPointer<QnLinkToCloudDialog> dialog(new QnLinkToCloudDialog(q));
    dialog->exec();
}

void QnCloudManagementWidgetPrivate::at_unlinkButton_clicked()
{
    Q_Q(QnCloudManagementWidget);
    QScopedPointer<QnUnlinkFromCloudDialog> dialog(new QnUnlinkFromCloudDialog(q));
    dialog->exec();
}

void QnCloudManagementWidgetPrivate::updateUi()
{
    QnUserResourcePtr adminUser = qnResPool->getAdministrator();

    bool linked = !adminUser->getProperty(Qn::CLOUD_SYSTEM_ID).isEmpty() &&
                  !adminUser->getProperty(Qn::CLOUD_SYSTEM_AUTH_KEY).isEmpty();

    Q_Q(QnCloudManagementWidget);

    if (linked)
    {
        const QColor nxColor(lit("#2fa2db")); // TODO: #dklychkov make customizeable

        QString accountLabel = lit("<span style=\"color:%2;\">%1</span>")
                               .arg(adminUser->getProperty(Qn::CLOUD_ACCOUNT_NAME))
                               .arg(nxColor.name());

        q->ui->linkedAccountInfoLabel->setText(tr("This system is linked to the cloud account %1").arg(accountLabel));
        q->ui->stackedWidget->setCurrentWidget(q->ui->linkedPage);
    }
    else
    {

        q->ui->stackedWidget->setCurrentWidget(q->ui->notLinkedPage);
    }
}
