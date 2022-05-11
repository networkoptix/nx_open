// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "advanced_update_settings_dialog.h"

#include <QtQuick/QQuickItem>
#include <QtWidgets/QWidget>

#include <nx/vms/client/core/utils/qml_property.h>
#include <nx/vms/client/desktop/system_update/client_update_manager.h>

#include <api/global_settings.h>
#include <common/common_module.h>
#include <ui/workbench/workbench_context.h>

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

    notifyAboutUpdates = qnGlobalSettings->isUpdateNotificationsEnabled();

    notifyAboutUpdates.connectNotifySignal(
        this,
        [this, notifyAboutUpdates]()
        {
            qnGlobalSettings->setUpdateNotificationsEnabled(notifyAboutUpdates);
            qnGlobalSettings->synchronizeNow();
        });

    connect(qnGlobalSettings,
        &QnGlobalSettings::updateNotificationsChanged,
        this,
        [this, notifyAboutUpdates]()
        {
            notifyAboutUpdates = qnGlobalSettings->isUpdateNotificationsEnabled();
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
