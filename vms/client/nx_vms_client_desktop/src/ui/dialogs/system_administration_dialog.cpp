// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_administration_dialog.h"
#include "ui_system_administration_dialog.h"

#include <QtWidgets/QPushButton>

#include <nx/branding.h>
#include <nx/network/rest/result.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/api/data/system_settings.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
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
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/saas/saas_utils.h>
#include <nx/vms/common/system_settings.h>
#include <ui/widgets/system_settings/cloud_management_widget.h>
#include <ui/widgets/system_settings/database_management_widget.h>
#include <ui/widgets/system_settings/general_system_administration_widget.h>
#include <ui/widgets/system_settings/license_manager_widget.h>
#include <ui/widgets/system_settings/routing_management_widget.h>
#include <ui/widgets/system_settings/saas_info_widget.h>

using namespace std::chrono;
using namespace nx::vms::client::desktop;

class QnSystemAdministrationDialog::Private
{
public:
    nx::vms::api::SaveableSystemSettings editableSystemSettings;
    rest::Handle currentRequest = 0;
};

QnSystemAdministrationDialog::QnSystemAdministrationDialog(QWidget* parent):
    base_type(parent),
    d(new Private()),
    ui(new ::Ui::QnSystemAdministrationDialog)
{
    ui->setupUi(this);
    setHelpTopic(this, HelpTopic::Id::Administration);

    auto generalWidget = new QnGeneralSystemAdministrationWidget(&d->editableSystemSettings, this);
    addPage(GeneralPage, generalWidget, tr("General"));

    auto userManagementWidget = new UserManagementTabWidget(windowContext(), this);
    addPage(UserManagement, userManagementWidget, tr("User Management"));

    // This is a page for updating many servers in one run.
    auto multiUpdatesWidget = new MultiServerUpdatesWidget(this);
    addPage(UpdatesPage, multiUpdatesWidget, tr("Updates"));

    addPage(LicensesPage, new LicenseManagerWidget(this), tr("Licenses"));
    addPage(SaasInfoPage, new SaasInfoWidget(system(), this), tr("Services"));

    const auto updateLicenseAndSaasInfoPagesVisibility =
        [this]
        {
            const bool saasInitialized = nx::vms::common::saas::saasInitialized(system());
            setPageVisible(LicensesPage, !saasInitialized);
            setPageVisible(SaasInfoPage, saasInitialized);
        };
    connect(system()->saasServiceManager(),
        &nx::vms::common::saas::ServiceManager::dataChanged,
        this,
        updateLicenseAndSaasInfoPagesVisibility);

    connect(systemContext()->globalSettings(),
        &nx::vms::common::SystemSettings::organizationIdChanged,
        this,
        updateLicenseAndSaasInfoPagesVisibility);

    auto outgoingMailSettingsWidget = new OutgoingMailSettingsWidget(
        &d->editableSystemSettings, this);
    addPage(MailSettingsPage, outgoingMailSettingsWidget, tr("Email"));

    auto securityWidget = new SecuritySettingsWidget(&d->editableSystemSettings, this);
    addPage(SecurityPage, securityWidget, tr("Security"));

    connect(securityWidget, &SecuritySettingsWidget::manageUsers, this,
        [this, userManagementWidget]
        {
            setCurrentPage(UserManagement);
            userManagementWidget->manageDigestUsers();
        });

    connect(system()->accessController(),
        &nx::vms::client::core::AccessController::globalPermissionsChanged,
        this,
        &QnSystemAdministrationDialog::updateSecurity);

    connect(system()->globalSettings(),
        &nx::vms::common::SystemSettings::securityForPowerUsersChanged,
        this,
        &QnSystemAdministrationDialog::updateSecurity);

    updateSecurity();

    addPage(CloudManagement, new QnCloudManagementWidget(this), nx::branding::cloudName());
    addPage(
        TimeServerSelection,
        new TimeSynchronizationWidget(&d->editableSystemSettings, this),
        tr("Time Sync"));

    auto routingWidget = new QnRoutingManagementWidget(this);
    addPage(RoutingManagement, routingWidget, tr("Routing"));

    if (!ini().integrationsManagement)
    {
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
    }


    auto advancedSettingsWidget = new AdvancedSystemSettingsWidget(system(), this);
    addPage(Advanced, advancedSettingsWidget, tr("Advanced"));

    loadDataToUi();
    updateLicenseAndSaasInfoPagesVisibility();

    autoResizePagesToContents(
        ui->tabWidget, {QSizePolicy::Preferred, QSizePolicy::Preferred}, true);
}

QnSystemAdministrationDialog::~QnSystemAdministrationDialog()
{
    if (!NX_ASSERT(!isNetworkRequestRunning(), "Requests should already be completed."))
        discardChanges();
}

bool QnSystemAdministrationDialog::isNetworkRequestRunning() const
{
    return base_type::isNetworkRequestRunning() || d->currentRequest > 0;
}

void QnSystemAdministrationDialog::applyChanges()
{
    d->editableSystemSettings = {};

    base_type::applyChanges();

    // Not all settings in the system administration dialog are saved through editableSystemSettings
    // some may be saved in applyChanges methods of the dialog pages themselves, such as Routing.
    if (d->editableSystemSettings.isEmpty())
        return;

    auto callback =
        [this, sessionLimitChanged = d->editableSystemSettings.sessionLimitS.has_value()](
            bool /*success*/,
            rest::Handle requestId,
            rest::ServerConnection::ErrorOrEmpty response)
        {
            NX_ASSERT(requestId == d->currentRequest || d->currentRequest == 0);
            d->currentRequest = 0;

            if (response)
            {
                const auto errors = systemSettings()->update(d->editableSystemSettings);
                if (!errors.empty())
                    NX_WARNING(this, "Failed to update system settings: %1", errors);
            }
            else
            {
                auto error = response.error();

                // If the session duration is set to be less than the current token's lifetime, the
                // settings will be applied. The current connection to the server will then be
                // terminated before we receive a response to the request. Upon disconnection, the
                // dialog will trigger discardChanges(). This will cause the request to be canceled
                // with a serviceUnavailable error. This error should not be displayed as a critical
                // message to the user.
                if (error.errorId == nx::network::rest::ErrorId::serviceUnavailable
                    && sessionLimitChanged)
                {
                    return;
                }
                NX_DEBUG(this, "Can't save system settings, code: %1, error: %2",
                    error.errorId, error.errorString);
                QnSessionAwareMessageBox::critical(
                    this, tr("Failed to save site settings"), error.errorString);
            }
        };

    d->currentRequest = connectedServerApi()->patchSystemSettings(
        systemContext()->getSessionTokenHelper(),
        d->editableSystemSettings,
        std::move(callback),
        this);
}

void QnSystemAdministrationDialog::discardChanges()
{
    base_type::discardChanges();
    if (auto api = connectedServerApi(); api && d->currentRequest > 0)
        api->cancelRequest(d->currentRequest);
    d->currentRequest = 0;
    NX_ASSERT(!isNetworkRequestRunning());
}

void QnSystemAdministrationDialog::updateSecurity()
{
    const auto pageIt = findPage(SecurityPage);
    if (!NX_ASSERT(pageIt != pages().cend()))
        return;

    const auto securityWidget = qobject_cast<SecuritySettingsWidget*>(pageIt->widget);
    if (!NX_ASSERT(securityWidget))
        return;

    const auto canEditSecuritySettings =
        [this]() -> bool
        {
            const auto globalPermissions = accessController()->globalPermissions();
            if (globalPermissions.testFlag(GlobalPermission::administrator))
                return true;
            if (!globalPermissions.testFlag(GlobalPermission::powerUser))
                return false;

            return globalSettings()->securityForPowerUsers();
        };

    const bool securityEnabled = canEditSecuritySettings();
    setPageVisible(SecurityPage, securityEnabled);

    if (!securityEnabled)
        securityWidget->resetChanges();
}
