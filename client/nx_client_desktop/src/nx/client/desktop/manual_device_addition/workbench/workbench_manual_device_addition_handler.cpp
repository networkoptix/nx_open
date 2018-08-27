#include "workbench_manual_device_addition_handler.h"

#include <QtWidgets/QAction>

#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/manual_device_addition/dialog/device_addition_dialog.h>

#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <ui/dialogs/common/non_modal_dialog_constructor.h>

namespace nx {
namespace client {
namespace desktop {

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

            QnNonModalDialogConstructor<DeviceAdditionDialog> dialogContructor(
                m_deviceAdditionDialog, mainWindowWidget());

            m_deviceAdditionDialog->setServer(server);
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

} // namespace desktop
} // namespace client
} // namespace nx

