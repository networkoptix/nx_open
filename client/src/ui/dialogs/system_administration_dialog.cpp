#include "system_administration_dialog.h"
#include "ui_system_administration_dialog.h"

#include <QtWidgets/QMessageBox>

#include <ui/widgets/settings/popup_settings_widget.h>
#include <ui/widgets/settings/license_manager_widget.h>
#include <ui/widgets/settings/camera_management_widget.h>
#include <ui/widgets/settings/smtp_settings_widget.h>
#include <ui/widgets/settings/database_management_widget.h>
#include <ui/widgets/settings/general_system_administration_widget.h>
#include <ui/widgets/server_updates_widget.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_state_manager.h>

QnSystemAdministrationDialog::QnSystemAdministrationDialog(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::QnSystemAdministrationDialog),
    m_workbenchStateDelegate(new QnBasicWorkbenchStateDelegate<QnSystemAdministrationDialog>(this))
{
    ui->setupUi(this);

    addPage(GeneralPage, new QnGeneralSystemAdministrationWidget(this), tr("General"));
    addPage(LicensesPage, new QnLicenseManagerWidget(this), tr("Licenses"));
    addPage(SmtpPage, new QnSmtpSettingsWidget(this), tr("Email"));
    addPage(UpdatesPage, new QnServerUpdatesWidget(this), tr("Updates"));

    loadData();
}

QnSystemAdministrationDialog::~QnSystemAdministrationDialog() {}
