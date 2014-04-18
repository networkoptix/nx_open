#include "system_administration_dialog.h"
#include "ui_system_administration_dialog.h"

#include <ui/widgets/server_updates_widget.h>

QnSystemAdministrationDialog::QnSystemAdministrationDialog(QnWorkbenchContext *context, QWidget *parent) :
    QDialog(parent),
    QnWorkbenchContextAware(context),
    ui(new Ui::QnSystemAdministrationDialog)
{
    ui->setupUi(this);

    QnServerUpdatesWidget *serverUpdatesWidget = new QnServerUpdatesWidget(context, this);
    ui->tabWidget->addTab(serverUpdatesWidget, tr("Updates"));
}

QnSystemAdministrationDialog::~QnSystemAdministrationDialog() {}
