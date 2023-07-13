// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_administration_dialog.h"
#include "ui_system_administration_dialog.h"

#include <QtWidgets/QPushButton>

#include <nx/branding.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/api/data/system_settings.h>
#include <nx/vms/client/core/settings/system_settings_manager.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_administration/widgets/advanced_system_settings_widget.h>
#include <nx/vms/client/desktop/system_administration/widgets/analytics_settings_widget.h>
#include <nx/vms/client/desktop/system_administration/widgets/outgoing_mail_settings_widget.h>
#include <nx/vms/client/desktop/system_administration/widgets/security_settings_widget.h>
#include <nx/vms/client/desktop/system_administration/widgets/time_synchronization_widget.h>
#include <nx/vms/client/desktop/system_administration/widgets/user_list_widget.h>
#include <nx/vms/client/desktop/system_administration/widgets/user_management_tab_widget.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_update/multi_server_updates_widget.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/widgets/system_settings/cloud_management_widget.h>
#include <ui/widgets/system_settings/database_management_widget.h>
#include <ui/widgets/system_settings/general_system_administration_widget.h>
#include <ui/widgets/system_settings/license_manager_widget.h>
#include <ui/widgets/system_settings/routing_management_widget.h>

using namespace std::chrono;
using namespace nx::vms::client::desktop;

QnSystemAdministrationDialog::QnSystemAdministrationDialog(QWidget* parent):
    base_type(parent),
    ui(new ::Ui::QnSystemAdministrationDialog)
{
    ui->setupUi(this);
    setHelpTopic(this, Qn::Administration_Help);

    auto generalWidget = new QnGeneralSystemAdministrationWidget(this);
    addPage(GeneralPage, generalWidget, tr("General"));

    auto userManagementWidget = new UserManagementTabWidget(
        systemContext()->userGroupManager(), this);
    addPage(UserManagement, userManagementWidget, tr("User Management"));
    connect(this, &QnGenericTabbedDialog::dialogClosed,
        this, [userManagementWidget]() { userManagementWidget->resetWarnings(); });

    // This is a page for updating many servers in one run.
    auto multiUpdatesWidget = new MultiServerUpdatesWidget(this);
    addPage(UpdatesPage, multiUpdatesWidget, tr("Updates"));

    addPage(LicensesPage, new LicenseManagerWidget(this), tr("Licenses"));

    auto outgoingMailSettingsWidget = new OutgoingMailSettingsWidget(systemContext(), this);
    addPage(MailSettingsPage, outgoingMailSettingsWidget, tr("Email"));

    auto securityWidget = new SecuritySettingsWidget(this);
    addPage(SecurityPage, securityWidget, tr("Security"));

    connect(securityWidget, &SecuritySettingsWidget::manageUsers, this,
        [this, userManagementWidget]
        {
            setCurrentPage(UserManagement);
            userManagementWidget->manageDigestUsers();
        });

    addPage(CloudManagement, new QnCloudManagementWidget(this), nx::branding::cloudName());
    addPage(
        TimeServerSelection,
        new TimeSynchronizationWidget(systemContext(), this),
        tr("Time Sync"));

    auto routingWidget = new QnRoutingManagementWidget(this);
    addPage(RoutingManagement, routingWidget, tr("Routing"));

    const auto analyticsSettingsWidget = new AnalyticsSettingsWidget(this);
    auto updateAnalyticsSettingsWidgetVisibility =
        [this, analyticsSettingsWidget]
        {
            setPageVisible(Analytics, analyticsSettingsWidget->shouldBeVisible());
        };

    addPage(
        Analytics,
        analyticsSettingsWidget,
        ini().enableMetadataApi ? tr("Integrations") : tr("Plugins"));

    connect(analyticsSettingsWidget, &AnalyticsSettingsWidget::visibilityUpdateRequested, this,
        updateAnalyticsSettingsWidgetVisibility);

    auto advancedSettingsWidget = new AdvancedSystemSettingsWidget(
        appContext()->currentSystemContext(), this);
    addPage(Advanced, advancedSettingsWidget, tr("Advanced"));

    loadDataToUi();
    updateAnalyticsSettingsWidgetVisibility();

    autoResizePagesToContents(
        ui->tabWidget, {QSizePolicy::Preferred, QSizePolicy::Preferred}, true);

    connect(this, &QnGenericTabbedDialog::dialogClosed,
        this, [securityWidget]() { securityWidget->resetWarnings(); });
}

QnSystemAdministrationDialog::~QnSystemAdministrationDialog()
{
    // Required here for forward-declared scoped pointer destruction.
}

bool QnSystemAdministrationDialog::confirmChangesOnExit()
{
    return true;
}

void QnSystemAdministrationDialog::applyChanges()
{
    for (const Page& page: modifiedPages())
    {
        if (page.enabled && page.visible && page.widget->hasChanges())
            page.widget->applyChanges();
    }

    const auto callback = nx::utils::guarded(this,
        [this] (bool success)
        {
            systemContext()->systemSettingsManager()->requestSystemSettings(
                [this, success]
                {
                    if (success)
                        loadDataToUi();

                    updateButtonBox();
                });
        });

    systemContext()->systemSettingsManager()->saveSystemSettings(callback, this);

    updateButtonBox();
}

void QnSystemAdministrationDialog::loadDataToUi()
{
    for (const Page& page: allPages())
        page.widget->loadDataToUi();
}
