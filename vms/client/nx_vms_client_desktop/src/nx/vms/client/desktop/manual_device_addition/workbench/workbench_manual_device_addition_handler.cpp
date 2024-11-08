// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_manual_device_addition_handler.h"

#include <QtGui/QAction>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/manual_device_addition/dialog/device_addition_dialog.h>
#include <nx/vms/client/desktop/manual_device_addition/dialog/new_device_addition_dialog.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/client/desktop/utils/parameter_helper.h>
#include <ui/dialogs/common/non_modal_dialog_constructor.h>

template<typename Dialog>
static void configureDialog(
    nx::vms::client::desktop::WorkbenchManualDeviceAdditionHandler* handler,
    QPointer<Dialog> dialog,
    const QnMediaServerResourcePtr server)
{
    dialog->setServer(server);
    const auto removeOnClose = [dialog]()
    {
        if (dialog)
            dialog->deleteLater();
    };

    // Prevents dialog controls blinking.
    QObject::connect(dialog, &QDialog::accepted, handler, removeOnClose);
    QObject::connect(dialog, &QDialog::rejected, handler, removeOnClose);
    QObject::connect(dialog, &QDialog::finished, handler, removeOnClose);
}

namespace nx::vms::client::desktop {

WorkbenchManualDeviceAdditionHandler::WorkbenchManualDeviceAdditionHandler(QObject* parent):
    base_type(parent)
{
    connect(action(menu::AddDeviceManuallyAction), &QAction::triggered, this,
        [this]()
        {
            const auto params = menu()->currentParameters(sender());
            const auto server = params.resource().dynamicCast<QnMediaServerResource>();
            if (!server)
                return;

            const auto parent = utils::extractParentWidget(params, mainWindowWidget());

            if (ini().newAddDevicesDialog)
            {
                QnNonModalDialogConstructor<NewDeviceAdditionDialog> dialogContructor(
                    m_newDeviceAdditionDialog, parent);
                configureDialog(this, m_newDeviceAdditionDialog, server);
            }
            else
            {
                QnNonModalDialogConstructor<DeviceAdditionDialog> dialogContructor(
                    m_deviceAdditionDialog, parent);
                configureDialog(this, m_deviceAdditionDialog, server);
            }
        });

    connect(action(menu::MainMenuAddDeviceManuallyAction), &QAction::triggered, this,
        [this]()
        {
            const auto servers =
                resourcePool()->getAllServers(nx::vms::api::ResourceStatus::online);
            if (servers.isEmpty())
            {
                NX_ASSERT(false, "No online servers for device searching");
                return;
            }

            menu()->triggerForced(menu::AddDeviceManuallyAction,
                menu::Parameters(servers.first()));
        });
}

} // namespace nx::vms::client::desktop
