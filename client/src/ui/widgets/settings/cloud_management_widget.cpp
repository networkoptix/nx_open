#include "cloud_management_widget.h"
#include "ui_cloud_management_widget.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource/param.h>
#include <core/resource/user_resource.h>

#include <ui/dialogs/link_to_cloud_dialog.h>
#include <ui/dialogs/unlink_from_cloud_dialog.h>

class QnCloudManagementWidgetPrivate : public QObject
{
    QnCloudManagementWidget *q_ptr;
    Q_DECLARE_PUBLIC(QnCloudManagementWidget)
    Q_DECLARE_TR_FUNCTIONS(QnCloudManagementWidget)

public:
    QnCloudManagementWidgetPrivate(QnCloudManagementWidget *parent);

    void at_linkButton_clicked();
    void at_unlinkButton_clicked();
    void at_goToCloudButton_clicked();

    void updateUi();
};

QnCloudManagementWidget::QnCloudManagementWidget(QWidget *parent)
    : base_type(parent)
    , ui(new Ui::CloudManagementWidget)
    , d_ptr(new QnCloudManagementWidgetPrivate(this))
{
    ui->setupUi(this);

    // TODO: #help Set help topic

    // TODO: #dklychkov Use customization-specific values
    QString cloudName = lit("Nx Cloud");
    QString cloudUrl = lit("http://www.networkoptix.com");

    ui->learnMoreLabel->setText(
            lit("<a href=\"%2\">%1</a>")
                .arg(tr("Learn more about %1").arg(cloudName))
                .arg(cloudUrl)
    );

    Q_D(QnCloudManagementWidget);

    connect(ui->linkButton,         &QPushButton::clicked,  d,  &QnCloudManagementWidgetPrivate::at_linkButton_clicked);
    connect(ui->unlinkButton,       &QPushButton::clicked,  d,  &QnCloudManagementWidgetPrivate::at_unlinkButton_clicked);
    connect(ui->goToCloudButton,    &QPushButton::clicked,  d,  &QnCloudManagementWidgetPrivate::at_goToCloudButton_clicked);

    if (QnUserResourcePtr admin = qnResPool->getAdministrator())
        connect(admin, &QnResource::resourceChanged, d, &QnCloudManagementWidgetPrivate::updateUi);
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

void QnCloudManagementWidgetPrivate::at_goToCloudButton_clicked()
{
}

void QnCloudManagementWidgetPrivate::updateUi()
{
    QnUserResourcePtr adminUser = qnResPool->getAdministrator();

    bool linked = !adminUser->getProperty(Qn::CLOUD_SYSTEM_ID).isEmpty() &&
                  !adminUser->getProperty(Qn::CLOUD_SYSTEM_AUTH_KEY).isEmpty();

    Q_Q(QnCloudManagementWidget);

    if (linked)
    {
        QString accountLink = lit("<a>%1</a>").arg(adminUser->getProperty(Qn::CLOUD_ACCOUNT_NAME));
        q->ui->linkedAccountInfoLabel->setText(tr("This system is linked to the cloud account %1").arg(accountLink));
        q->ui->stackedWidget->setCurrentWidget(q->ui->linkedPage);
    }
    else
    {

        q->ui->stackedWidget->setCurrentWidget(q->ui->notLinkedPage);
    }
}
