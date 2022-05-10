// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_administration_dialog.h"
#include "ui_system_administration_dialog.h"

#include <QtWidgets/QPushButton>

#include <common/common_module.h>
#include <nx/branding.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_administration/widgets/analytics_settings_widget.h>
#include <nx/vms/client/desktop/system_administration/widgets/security_settings_widget.h>
#include <nx/vms/client/desktop/system_administration/widgets/time_synchronization_widget.h>
#include <nx/vms/client/desktop/system_administration/widgets/user_list_widget.h>
#include <nx/vms/client/desktop/system_update/multi_server_updates_widget.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/widgets/system_settings/cloud_management_widget.h>
#include <ui/widgets/system_settings/database_management_widget.h>
#include <ui/widgets/system_settings/general_system_administration_widget.h>
#include <ui/widgets/system_settings/license_manager_widget.h>
#include <ui/widgets/system_settings/routing_management_widget.h>
#include <ui/widgets/system_settings/smtp/smtp_settings_widget.h>

using namespace nx::vms::client::desktop;

QnSystemAdministrationDialog::QnSystemAdministrationDialog(QWidget* parent):
    base_type(parent),
    ui(new Ui::QnSystemAdministrationDialog)
{
    ui->setupUi(this);
    setHelpTopic(this, Qn::Administration_Help);

    auto okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    auto applyButton = ui->buttonBox->button(QDialogButtonBox::Apply);

    auto generalWidget = new QnGeneralSystemAdministrationWidget(this);
    addPage(GeneralPage, generalWidget, tr("General"));

    auto usersWidget = new UserListWidget(this);
    addPage(UserManagement, usersWidget, tr("Users"));

    // This is a page for updating many servers in one run.
    auto multiUpdatesWidget = new MultiServerUpdatesWidget(this);
    addPage(UpdatesPage, multiUpdatesWidget, tr("Updates"));

    addPage(LicensesPage, new QnLicenseManagerWidget(this), tr("Licenses"));

    auto smtpWidget = new QnSmtpSettingsWidget(this);
    addPage(SmtpPage, smtpWidget, tr("Email"));

    auto securityWidget = new SecuritySettingsWidget(this);
    addPage(SecurityPage, securityWidget, tr("Security"));
    connect(securityWidget, &SecuritySettingsWidget::manageUsers, this,
        [this, usersWidget]
        {
            setCurrentPage(UserManagement);
            usersWidget->filterDigestUsers();
        });

    addPage(CloudManagement, new QnCloudManagementWidget(this), nx::branding::cloudName());
    addPage(
        TimeServerSelection,
        new TimeSynchronizationWidget(this),
        tr("Time Synchronization"));

    auto routingWidget = new QnRoutingManagementWidget(this);
    addPage(RoutingManagement, routingWidget, tr("Routing Management"));

    const auto analyticsSettingsWidget = new AnalyticsSettingsWidget(this);
    auto updateAnalyticsSettingsWidgetVisibility =
        [this, analyticsSettingsWidget]
        {
            setPageVisible(Analytics, analyticsSettingsWidget->shouldBeVisible());
        };

    addPage(Analytics, analyticsSettingsWidget, tr("Plugins"));
    connect(analyticsSettingsWidget, &AnalyticsSettingsWidget::visibilityUpdateRequested, this,
        updateAnalyticsSettingsWidgetVisibility);

    loadDataToUi();
    updateAnalyticsSettingsWidgetVisibility();

    autoResizePagesToContents(ui->tabWidget, {QSizePolicy::Preferred, QSizePolicy::Preferred}, true);

    connect(this, &QnGenericTabbedDialog::dialogClosed,
        this, [securityWidget]() { securityWidget->resetWarnings(); });
}

QnSystemAdministrationDialog::~QnSystemAdministrationDialog()
{
    // Required here for forward-declared scoped pointer destruction.
}
