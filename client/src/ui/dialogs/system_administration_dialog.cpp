#include "system_administration_dialog.h"
#include "ui_system_administration_dialog.h"

#include <QtWidgets/QMessageBox>

#include <ui/widgets/settings/popup_settings_widget.h>
#include <ui/widgets/settings/license_manager_widget.h>
#include <ui/widgets/settings/camera_management_widget.h>
#include <ui/widgets/settings/smtp_settings_widget.h>
#include <ui/widgets/settings/database_management_widget.h>
#include <ui/widgets/server_updates_widget.h>

QnSystemAdministrationDialog::QnSystemAdministrationDialog(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::QnSystemAdministrationDialog)
{
    ui->setupUi(this);

    addPage(LicensesPage, new QnLicenseManagerWidget(this), tr("Licenses"));
    addPage(SmtpPage, new QnSmtpSettingsWidget(this), tr("Email"));
    addPage(CamerasPage, new QnCameraManagementWidget(this), tr("Cameras"));
    addPage(UpdatesPage, new QnServerUpdatesWidget(this), tr("Updates"));
    addPage(BackupPage, new QnDatabaseManagementWidget(this), tr("Backup && Restore"));

    loadData();
}
