#include "workbench_manual_device_addition_handler.h"

#include <QtWidgets/QAction>

#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/manual_device_addition/dialog/device_addition_dialog.h>
#include <nx/vms/client/desktop/utils/parameter_helper.h>

#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <ui/dialogs/common/non_modal_dialog_constructor.h>


namespace nx::vms::client::desktop {

WorkbenchManualDeviceAdditionHandler::WorkbenchManualDeviceAdditionHandler(QObject* parent):
    base_type(parent)
{
    connect(action(ui::action::AddDeviceManuallyAction), &QAction::triggered, this,
        [this]()
        {
            const auto params = menu()->currentParameters(sender());
            const auto server = params.resource().dynamicCast<QnMediaServerResource>();
            if (!server)
                return;

            const auto parent = utils::extractParentWidget(params, mainWindowWidget());

            QnNonModalDialogConstructor<DeviceAdditionDialog> dialogContructor(
                m_deviceAdditionDialog, parent);

            m_deviceAdditionDialog->setServer(server);
            const auto removeOnClose =
                [this]()
                {
                    if (m_deviceAdditionDialog)
                        m_deviceAdditionDialog->deleteLater();
                };

            // Prevents dialog controls blinking.
            connect(m_deviceAdditionDialog, &QDialog::accepted, this, removeOnClose);
            connect(m_deviceAdditionDialog, &QDialog::rejected, this, removeOnClose);
            connect(m_deviceAdditionDialog, &QDialog::finished, this, removeOnClose);
        });

    connect(action(ui::action::MainMenuAddDeviceManuallyAction), &QAction::triggered, this,
        [this]()
        {
            const auto servers = commonModule()->resourcePool()->getAllServers(Qn::Online);
            if (servers.isEmpty())
            {
                NX_ASSERT(false, "No online servers for device searching");
                return;
            }

            menu()->triggerForced(ui::action::AddDeviceManuallyAction,
                ui::action::Parameters(servers.first()));
        });
}

} // namespace nx::vms::client::desktop

