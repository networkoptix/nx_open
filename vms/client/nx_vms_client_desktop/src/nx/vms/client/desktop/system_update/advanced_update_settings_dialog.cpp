// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "advanced_update_settings_dialog.h"

#include <QtQuick/QQuickItem>
#include <QtWidgets/QWidget>

#include <common/common_module.h>
#include <nx/vms/client/core/utils/qml_property.h>
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
        context()->findInstance<ClientUpdateManager>();

    QmlProperty<bool> notifyAboutUpdates(rootObjectHolder(), "notifyAboutUpdates");

    notifyAboutUpdates = systemSettings()->isUpdateNotificationsEnabled();

    notifyAboutUpdates.connectNotifySignal(
        this,
        [this, notifyAboutUpdates]()
        {
            systemSettings()->setUpdateNotificationsEnabled(notifyAboutUpdates);
            systemSettings()->synchronizeNow();
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
    reject();
    return true;
}

void AdvancedUpdateSettingsDialog::forcedUpdate()
{
}

} // namespace nx::vms::client::desktop
