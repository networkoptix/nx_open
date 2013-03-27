#include "license_notification_dialog.h"
#include "ui_license_notification_dialog.h"

#include <ui/models/license_list_model.h>
#include "utils/common/synctime.h"


QnLicenseNotificationDialog::QnLicenseNotificationDialog(QWidget *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    ui(new Ui::LicenseNotificationDialog)
{
    ui->setupUi(this);

    QList<QnLicenseListModel::Column> columns;
    columns << QnLicenseListModel::TypeColumn << QnLicenseListModel::CameraCountColumn << QnLicenseListModel::LicenseKeyColumn << QnLicenseListModel::ExpiresInColumn;

    m_model = new QnLicenseListModel(this);
    m_model->setColumns(columns);
    ui->treeView->setModel(m_model);
}

QnLicenseNotificationDialog::~QnLicenseNotificationDialog() {
    return;
}

const QList<QnLicensePtr> &QnLicenseNotificationDialog::licenses() const {
    return m_model->licenses();
}

void QnLicenseNotificationDialog::setLicenses(const QList<QnLicensePtr> &licenses) {
    m_model->setLicenses(licenses);

    // TODO: #Elric this code does not belong here.
    QDateTime now = qnSyncTime->currentDateTime();
    int expiredCount = 0;
    foreach(const QnLicensePtr &license, licenses)
        if(license->expirationDate() < now)
            expiredCount++;

    if(expiredCount > 0) {
        ui->label->setText(tr("Some of your licenses have expired."));
    } else {
        ui->label->setText(tr("Some of your licenses will soon expire."));
    }
}
