#include "system_administration_dialog.h"
#include "ui_system_administration_dialog.h"

#include <QtWidgets/QMessageBox>

#include <ui/widgets/settings/popup_settings_widget.h>
#include <ui/widgets/settings/license_manager_widget.h>
#include <ui/widgets/settings/camera_management_widget.h>
#include <ui/widgets/settings/smtp_settings_widget.h>
#include <ui/widgets/settings/database_management_widget.h>
#include <ui/widgets/settings/time_server_selection_widget.h>
#include <ui/widgets/settings/general_system_administration_widget.h>
#include <ui/widgets/server_updates_widget.h>
#include <ui/widgets/routing_management_widget.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_state_manager.h>

QnSystemAdministrationDialog::QnSystemAdministrationDialog(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::QnSystemAdministrationDialog),
    m_workbenchStateDelegate(new QnBasicWorkbenchStateDelegate<QnSystemAdministrationDialog>(this))
{
    ui->setupUi(this);

    m_updatesWidget = new QnServerUpdatesWidget(this);

    addPage(GeneralPage, new QnGeneralSystemAdministrationWidget(this), tr("General"));
    addPage(LicensesPage, new QnLicenseManagerWidget(this), tr("Licenses"));
    addPage(SmtpPage, new QnSmtpSettingsWidget(this), tr("Email"));
    addPage(UpdatesPage, m_updatesWidget, tr("Updates"));
    addPage(RoutingManagement, new QnRoutingManagementWidget(this), tr("Routing Management"));
    addPage(TimeServerSelection, new QnTimeServerSelectionWidget(this), tr("Time Synchronization"));

    loadData();
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
