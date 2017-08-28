#include "radass_action_handler.h"

#include <QtWidgets/QAction>

#include <nx/client/desktop/radass/radass_types.h>
#include <nx/client/desktop/radass/radass_controller.h>
#include <nx/client/desktop/radass/radass_resource_manager.h>

#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_manager.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>

namespace nx {
namespace client {
namespace desktop {

RadassActionHandler::RadassActionHandler(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    context()->instance<RadassResourceManager>();

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

