#include "system_administration_dialog.h"
#include "ui_system_administration_dialog.h"

#include <QtWidgets/QMessageBox>

#include <ui/widgets/settings/license_manager_widget.h>
#include <ui/widgets/settings/server_settings_widget.h>
#include <ui/widgets/server_updates_widget.h>

QnSystemAdministrationDialog::QnSystemAdministrationDialog(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::QnSystemAdministrationDialog)
{
    ui->setupUi(this);

    addPage(LicensesPage, new QnLicenseManagerWidget(this), tr("Licenses"));
    addPage(ServerPage, new QnServerSettingsWidget(this), tr("Server"));
    addPage(UpdatesPage, new QnServerUpdatesWidget(this), tr("Updates"));

    loadData();
}
