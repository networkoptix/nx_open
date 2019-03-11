#include "system_administration_dialog.h"
#include "ui_system_administration_dialog.h"

#include <QtWidgets/QPushButton>

#include <nx/network/app_info.h>

#include <common/common_module.h>

#include <api/global_settings.h>

#include <ui/widgets/system_settings/license_manager_widget.h>
#include <ui/widgets/system_settings/database_management_widget.h>
#include <ui/widgets/system_settings/time_server_selection_widget.h>
#include <ui/widgets/system_settings/general_system_administration_widget.h>
#include <ui/widgets/system_settings/user_management_widget.h>
#include <ui/widgets/system_settings/cloud_management_widget.h>
#include <ui/widgets/system_settings/routing_management_widget.h>
#include <ui/widgets/system_settings/smtp/smtp_settings_widget.h>
#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/style/custom_style.h>
#include <ui/workbench/watchers/workbench_safemode_watcher.h>
#include <nx/vms/client/desktop/system_update/multi_server_updates_widget.h>
#include <nx/vms/client/desktop/system_administration/widgets/time_synchronization_widget.h>
#include <nx/vms/client/desktop/system_administration/widgets/analytics_settings_widget.h>

#include <nx/vms/client/desktop/ini.h>

using namespace nx::vms::client::desktop;

QnSystemAdministrationDialog::QnSystemAdministrationDialog(QWidget* parent):
    base_type(parent),
    ui(new Ui::QnSystemAdministrationDialog)
{
    ui->setupUi(this);
    setHelpTopic(this, Qn::Administration_Help);

    auto okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    auto applyButton = ui->buttonBox->button(QDialogButtonBox::Apply);

    QnWorkbenchSafeModeWatcher* safeModeWatcher = new QnWorkbenchSafeModeWatcher(this);
    safeModeWatcher->addWarningLabel(ui->buttonBox);
    safeModeWatcher->addControlledWidget(okButton, QnWorkbenchSafeModeWatcher::ControlMode::Disable);

    auto generalWidget = new QnGeneralSystemAdministrationWidget(this);
    auto smtpWidget = new QnSmtpSettingsWidget(this);
    auto routingWidget = new QnRoutingManagementWidget(this);
    addPage(GeneralPage,            generalWidget,                          tr("General"));
    addPage(LicensesPage,           new QnLicenseManagerWidget(this),       tr("Licenses"));
    addPage(SmtpPage,               smtpWidget,                             tr("Email"));

    // This is a page for updating many servers in one run.
    auto multiUpdatesWidget = new MultiServerUpdatesWidget(this);
    addPage(UpdatesPage, multiUpdatesWidget, tr("Updates"));
    safeModeWatcher->addControlledWidget(multiUpdatesWidget,
        QnWorkbenchSafeModeWatcher::ControlMode::Disable);

    addPage(UserManagement,         new QnUserManagementWidget(this),       tr("Users"));
    addPage(RoutingManagement,      routingWidget,                          tr("Routing Management"));
    if (ini().redesignedTimeSynchronization)
    {
        addPage(
            TimeServerSelection,
            new TimeSynchronizationWidget(this),
            tr("Time Synchronization"));
    }
    else
    {
        addPage(
            TimeServerSelection,
            new QnTimeServerSelectionWidget(this),
            tr("Time Synchronization"));
    }

    addPage(CloudManagement,        new QnCloudManagementWidget(this),      nx::network::AppInfo::cloudName());
    addPage(Analytics,              new AnalyticsSettingsWidget(this),      tr("Analytics"));

    loadDataToUi();
    autoResizePagesToContents(ui->tabWidget, {QSizePolicy::Preferred, QSizePolicy::Preferred}, true);

    /* Hiding Apply button, otherwise it will be enabled in the QnGenericTabbedDialog code */
    safeModeWatcher->addControlledWidget(applyButton, QnWorkbenchSafeModeWatcher::ControlMode::Hide);
    safeModeWatcher->addControlledWidget(generalWidget, QnWorkbenchSafeModeWatcher::ControlMode::MakeReadOnly);
    safeModeWatcher->addControlledWidget(smtpWidget, QnWorkbenchSafeModeWatcher::ControlMode::MakeReadOnly);
    safeModeWatcher->addControlledWidget(routingWidget, QnWorkbenchSafeModeWatcher::ControlMode::MakeReadOnly);

    connect(this, &QnGenericTabbedDialog::dialogClosed,
        this, [generalWidget]() { generalWidget->resetWarnings(); });
}

QnSystemAdministrationDialog::~QnSystemAdministrationDialog() = default;
