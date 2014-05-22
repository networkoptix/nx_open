#include "system_administration_dialog.h"
#include "ui_system_administration_dialog.h"

#include <QtWidgets/QMessageBox>

#include <ui/widgets/server_updates_widget.h>

QnSystemAdministrationDialog::QnSystemAdministrationDialog(QnWorkbenchContext *context, QWidget *parent) :
    QDialog(parent),
    QnWorkbenchContextAware(context),
    ui(new Ui::QnSystemAdministrationDialog)
{
    ui->setupUi(this);

    m_updatesWidget = new QnServerUpdatesWidget(this);
    ui->tabWidget->addTab(m_updatesWidget, tr("Updates"));
}

QnSystemAdministrationDialog::~QnSystemAdministrationDialog() {}

void QnSystemAdministrationDialog::reject() {
    if (!m_updatesWidget->cancelUpdate()) {
        QMessageBox::critical(this, tr("Error"), tr("Cannot cancel update at this state."));
        return;
    }
    base_type::reject();
}

void QnSystemAdministrationDialog::accept() {
    if (m_updatesWidget->isUpdating()) {
        QMessageBox::information(this, tr("Information"), tr("Update is in process now."));
        return;
    }
    base_type::accept();
}
