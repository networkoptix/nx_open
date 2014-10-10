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
    columns << QnLicenseListModel::TypeColumn << QnLicenseListModel::CameraCountColumn << QnLicenseListModel::LicenseKeyColumn  << QnLicenseListModel::LicenseStatusColumn;

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
    qint64 currentTime = qnSyncTime->currentMSecsSinceEpoch();
    int invalidCount = 0;
    foreach(const QnLicensePtr &license, licenses)
        if (!qnLicensePool->isLicenseValid(license))
            invalidCount++;

    if(invalidCount > 0)
        ui->label->setText(tr("Some of your licenses are unavailable."));
    else
        ui->label->setText(tr("Some of your licenses will soon expire."));

    for(int c = 0; c < ui->treeView->model()->columnCount(); c++)
        ui->treeView->resizeColumnToContents(c);
}
