// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "integration_settings_watcher.h"

#include <client/client_globals.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/action_parameters.h>
#include <nx/vms/client/desktop/system_administration/dialogs/integrations_dialog.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/extensions/local_notifications_manager.h>
#include <nx/vms/common/system_settings.h>

namespace nx::vms::client::desktop {

IntegrationSettingsWatcher::IntegrationSettingsWatcher(SystemContext* context, QObject* parent):
    QObject(parent),
    SystemContextAware(context)
{
    if (!ini().enableMetadataApi)
        return;

    auto openSettings =
        [this](const nx::Uuid& notificationId)
        {
            if (m_notificationId == notificationId)
            {
                appContext()->mainWindowContext()->menu()->trigger(
                    menu::OpenIntegrationsAction,
                    menu::Parameters{Qn::FocusTabRole, IntegrationsDialog::Tab::settings});
            }
        };

    auto updateNotification =
        [this, openSettings]()
        {
            const bool isPowerUser = accessController()->hasPowerUserPermissions();
            const bool isRegisteringAllowed =
                globalSettings()->isAllowRegisteringIntegrationsEnabled();

            const bool showNotification = isPowerUser && isRegisteringAllowed;
            const bool isNotificationVisible = m_notificationId.has_value();

            if (showNotification == isNotificationVisible)
                return;

            const auto notificationsManager =
                appContext()->mainWindowContext()->localNotificationsManager();

            if (showNotification)
            {
                m_notificationId = notificationsManager->add(
                    tr("API Integrations approval requests are permitted"),
                    tr("API Integrations registration requests are enabled. While safeguards "
                        "exist, prolonged usage is not recommended. Consider disabling this "
                        "option after all necessary Integrations are installed."),
                    /*cancellable*/ false);

                notificationsManager->setLevel(
                    *m_notificationId, nx::vms::event::Level::important);

                m_connection.reset(connect(
                    notificationsManager,
                    &workbench::LocalNotificationsManager::interactionRequested,
                    this,
                    openSettings));
            }
            else
            {
                notificationsManager->remove(*m_notificationId);
                m_notificationId = {};
                m_connection.reset();
            }
        };

    connect(globalSettings(), &common::SystemSettings::allowRegisteringIntegrationsChanged,
        this, updateNotification);

    updateNotification();
}

} // namespace nx::vms::client::desktop
