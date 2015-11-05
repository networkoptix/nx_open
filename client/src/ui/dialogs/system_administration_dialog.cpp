#include "system_administration_dialog.h"
#include "ui_system_administration_dialog.h"

#include <QtWidgets/QMessageBox>

#include <common/common_module.h>

#include <ui/widgets/settings/popup_settings_widget.h>
#include <ui/widgets/settings/license_manager_widget.h>
#include <ui/widgets/settings/system_settings_widget.h>
#include <ui/widgets/settings/database_management_widget.h>
#include <ui/widgets/settings/time_server_selection_widget.h>
#include <ui/widgets/settings/general_system_administration_widget.h>
#include <ui/widgets/settings/user_management_widget.h>
#include <ui/widgets/settings/cloud_management_widget.h>
#include <ui/widgets/server_updates_widget.h>
#include <ui/widgets/routing_management_widget.h>

#include <ui/widgets/system_settings/smtp_settings_widget.h>

#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_state_manager.h>
#include <ui/workbench/watchers/workbench_safemode_watcher.h>

QnSystemAdministrationDialog::QnSystemAdministrationDialog(QWidget *parent) 
    : base_type(parent)
    , ui(new Ui::QnSystemAdministrationDialog)
{
    ui->setupUi(this);
    setHelpTopic(this, Qn::Administration_Help);

    auto updatesWidget = new QnServerUpdatesWidget(this);
    auto generalWidget = new QnGeneralSystemAdministrationWidget(this);
    auto smtpWidget = new QnSmtpSettingsWidget(this);
    auto routingWidget = new QnRoutingManagementWidget(this);

    addPage(GeneralPage,            generalWidget,                          tr("General"));
    addPage(LicensesPage,           new QnLicenseManagerWidget(this),       tr("Licenses"));
    addPage(SmtpPage,               smtpWidget,                             tr("Email"));
    addPage(UpdatesPage,            updatesWidget,                          tr("Updates"));
    addPage(UserManagement,         new QnUserManagementWidget(this),       tr("Users"));
    addPage(RoutingManagement,      routingWidget,                          tr("Routing Management"));
    addPage(TimeServerSelection,    new QnTimeServerSelectionWidget(this),  tr("Time Synchronization"));
    addPage(CloudManagement,        new QnCloudManagementWidget(this),      tr("Cloud Management"));

    loadDataToUi();

    auto okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    auto applyButton = ui->buttonBox->button(QDialogButtonBox::Apply);

    QnWorkbenchSafeModeWatcher* safeModeWatcher = new QnWorkbenchSafeModeWatcher(this);
    safeModeWatcher->addWarningLabel(ui->buttonBox);
    safeModeWatcher->addControlledWidget(okButton, QnWorkbenchSafeModeWatcher::ControlMode::Disable);

    /* Hiding Apply button, otherwise it will be enabled in the QnGenericTabbedDialog code */
    safeModeWatcher->addControlledWidget(applyButton, QnWorkbenchSafeModeWatcher::ControlMode::Hide);
    safeModeWatcher->addControlledWidget(updatesWidget, QnWorkbenchSafeModeWatcher::ControlMode::Disable);
    safeModeWatcher->addControlledWidget(generalWidget, QnWorkbenchSafeModeWatcher::ControlMode::MakeReadOnly);
    safeModeWatcher->addControlledWidget(smtpWidget, QnWorkbenchSafeModeWatcher::ControlMode::MakeReadOnly);
    safeModeWatcher->addControlledWidget(routingWidget, QnWorkbenchSafeModeWatcher::ControlMode::MakeReadOnly);

    connect(qnCommon, &QnCommonModule::readOnlyChanged, this, [this](bool readOnly){
        setPageEnabled(UpdatesPage, !readOnly);
    });
    setPageEnabled(UpdatesPage, !qnCommon->isReadOnly());
}

QnSystemAdministrationDialog::~QnSystemAdministrationDialog()
{}
