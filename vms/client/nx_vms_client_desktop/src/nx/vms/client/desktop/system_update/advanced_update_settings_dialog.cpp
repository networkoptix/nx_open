// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "advanced_update_settings_dialog.h"

#include <QtQuick/QQuickItem>
#include <QtWidgets/QWidget>

#include <api/server_rest_connection.h>
#include <nx/vms/client/core/utils/qml_property.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_update/client_update_manager.h>
#include <nx/vms/common/system_settings.h>
#include <ui/workbench/workbench_context.h>

using namespace nx::vms::common;

namespace nx::vms::client::desktop {

AdvancedUpdateSettingsDialog::AdvancedUpdateSettingsDialog(QWidget* parent):
    QmlDialogWrapper(
        QUrl("Nx/Dialogs/SystemSettings/AdvancedUpdateSettings.qml"),
        {},
        parent),
    QnSessionAwareDelegate(parent)
{
    QmlProperty<ClientUpdateManager*>(rootObjectHolder(), "clientUpdateManager") =
        workbenchContext()->findInstance<ClientUpdateManager>();
    QmlProperty<bool> notifyAboutUpdates(rootObjectHolder(), "notifyAboutUpdates");

    notifyAboutUpdates = systemSettings()->isUpdateNotificationsEnabled();

    notifyAboutUpdates.connectNotifySignal(
        this,
        [this, notifyAboutUpdates]()
        {
            NX_ASSERT(m_currentRequest == 0);

            auto callback =
                [this](bool /*success*/,
                    rest::Handle requestId,
                    rest::ServerConnection::ErrorOrEmpty)
                {
                    NX_ASSERT(m_currentRequest == requestId || m_currentRequest == 0);
                    m_currentRequest = 0;
                };

            m_currentRequest = connectedServerApi()->patchSystemSettings(
                systemContext()->getSessionTokenHelper(),
                api::SaveableSystemSettings{.updateNotificationsEnabled = notifyAboutUpdates},
                callback,
                this);
        });

    connect(systemSettings(),
        &SystemSettings::updateNotificationsChanged,
        this,
        [this, notifyAboutUpdates]()
        {
            notifyAboutUpdates = systemSettings()->isUpdateNotificationsEnabled();
        });
}

bool AdvancedUpdateSettingsDialog::tryClose(bool /*force*/)
{
    if (auto api = connectedServerApi(); api && m_currentRequest > 0)
        api->cancelRequest(m_currentRequest);
    m_currentRequest = 0;

    reject();
    return true;
}

} // namespace nx::vms::client::desktop
