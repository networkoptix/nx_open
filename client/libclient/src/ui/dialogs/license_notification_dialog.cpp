#include "license_notification_dialog.h"
#include "ui_license_notification_dialog.h"

#include <ui/models/license_list_model.h>
#include "utils/common/synctime.h"
#include <ui/widgets/common/snapped_scrollbar.h>


QnLicenseNotificationDialog::QnLicenseNotificationDialog(QWidget *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    ui(new Ui::LicenseNotificationDialog)
{
    ui->setupUi(this);

    QnSnappedScrollBar *scrollBar = new QnSnappedScrollBar(this);
    ui->treeView->setVerticalScrollBar(scrollBar->proxyScrollBar());

    m_model = new QnLicenseListModel(this);
    ui->treeView->setColumnHidden(QnLicenseListModel::ExpirationDateColumn, true);
    ui->treeView->setColumnHidden(QnLicenseListModel::ServerColumn, true);
    ui->treeView->setModel(m_model);
}

QnLicenseNotificationDialog::~QnLicenseNotificationDialog()
{}

void QnLicenseNotificationDialog::setLicenses(const QnLicenseList& licenses)
{
    m_model->updateLicenses(licenses);

    if (std::any_of(licenses.cbegin(), licenses.cend(), [](const QnLicensePtr& license) { return !license->isValid(); }))
        ui->label->setText(tr("Some of your licenses are unavailable."));
    else
        ui->label->setText(tr("Some of your licenses will soon expire."));

    for(int c = 0; c < ui->treeView->model()->columnCount(); c++)
        ui->treeView->resizeColumnToContents(c);
}
