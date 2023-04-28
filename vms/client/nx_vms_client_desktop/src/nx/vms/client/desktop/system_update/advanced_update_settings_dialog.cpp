// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "advanced_update_settings_dialog.h"

#include <QtQuick/QQuickItem>
#include <QtWidgets/QWidget>

#include <common/common_module.h>
#include <nx/vms/api/data/system_settings.h>
#include <nx/vms/client/core/utils/qml_property.h>
#include <nx/vms/client/core/settings/system_settings_manager.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_update/client_update_manager.h>
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
        context()->findInstance<ClientUpdateManager>();
    QmlProperty<bool> notifyAboutUpdates(rootObjectHolder(), "notifyAboutUpdates");

    const auto systemSetting = systemContext()->systemSettings();
    notifyAboutUpdates = systemSetting->updateNotificationsEnabled;

    notifyAboutUpdates.connectNotifySignal(
        this,
        [this, notifyAboutUpdates, systemSetting]()
        {
            systemSetting->updateNotificationsEnabled = notifyAboutUpdates;
            systemContext()->systemSettingsManager()->saveSystemSettings();
        });

    connect(systemContext()->systemSettingsManager(),
        &core::SystemSettingsManager::systemSettingsChanged,
        this,
        [this, notifyAboutUpdates, systemSetting]()
        {
            notifyAboutUpdates = systemSetting->updateNotificationsEnabled;
        });
}

bool AdvancedUpdateSettingsDialog::tryClose(bool /*force*/)
{
    reject();
    return true;
}

void AdvancedUpdateSettingsDialog::forcedUpdate()
{
}

} // namespace nx::vms::client::desktop
