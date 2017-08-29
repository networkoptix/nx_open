#include "radass_action_handler.h"

#include <QtWidgets/QAction>

#include <client/client_module.h>

#include <nx/client/desktop/radass/radass_types.h>
#include <nx/client/desktop/radass/radass_controller.h>
#include <nx/client/desktop/radass/radass_resource_manager.h>
#include <nx/client/desktop/radass/radass_cameras_watcher.h>

#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_manager.h>


#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>

namespace nx {
namespace client {
namespace desktop {

struct RadassActionHandler::Private
{
    QScopedPointer<RadassCamerasWatcher> camerasWatcher;
    RadassController* controller = nullptr;
    RadassResourceManager* manager = nullptr;
};

RadassActionHandler::RadassActionHandler(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d(new Private)
{
    d->controller = qnClientModule->radassController();
    d->camerasWatcher.reset(new RadassCamerasWatcher(d->controller, resourcePool()));
    // Manager must be available from actions factory.
    d->manager = context()->instance<RadassResourceManager>();

    connect(action(ui::action::RadassAction), &QAction::triggered, this,
        &RadassActionHandler::at_radassAction_triggered);
}

RadassActionHandler::~RadassActionHandler()
{

}

void RadassActionHandler::at_radassAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());

    const auto mode = static_cast<RadassMode>(parameters.argument(Qn::ResolutionModeRole).toInt());
    if (mode == RadassMode::Custom)
        return;

    const auto manager = context()->instance<RadassResourceManager>();

    // If empty, means apply to the current layout.
    const auto layoutItems = parameters.layoutItems();
    if (layoutItems.empty())
        manager->setMode(workbench()->currentLayout()->resource(), mode);
    else
        manager->setMode(layoutItems, mode);
}

} // namespace desktop
} // namespace client
} // namespace nx

