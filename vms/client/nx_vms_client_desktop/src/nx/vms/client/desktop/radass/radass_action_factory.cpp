// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "radass_action_factory.h"

#include <algorithm>

#include <QtGui/QAction>
#include <QtGui/QActionGroup>

#include <camera/cam_display.h>
#include <nx/vms/client/core/resource/layout_resource.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/action_parameters.h>
#include <nx/vms/client/desktop/radass/radass_controller.h>
#include <nx/vms/client/desktop/radass/radass_resource_manager.h>
#include <nx/vms/client/desktop/radass/radass_support.h>
#include <nx/vms/client/desktop/radass/radass_types.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_layout.h>

namespace nx::vms::client::desktop {
namespace menu {

RadassActionFactory::RadassActionFactory(Manager* parent):
    base_type(parent)
{
}

QList<QAction*> RadassActionFactory::newActions(const Parameters& parameters,
    QObject* parent)
{
    auto actionGroup = new QActionGroup(parent);
    actionGroup->setExclusive(true);

    const auto manager = windowContext()->system()->radassResourceManager();

    // If no layout items are provided, using current layout.
    auto items = parameters.layoutItems();
    const auto currentMode = items.empty()
        ? manager->mode(workbench()->currentLayoutResource())
        : manager->mode(items);

    // The stored mode may be stale: fullscreen, zoom window and fisheye dewarping force the
    // stream to High quality regardless of it. Show the actually streamed quality instead.
    auto effectiveMode = currentMode;
    if (currentMode == RadassMode::Low && !items.empty())
    {
        auto supportedItems =
            items | std::views::filter([](const auto& item) { return isRadassSupported(item); });

        const auto controller = appContext()->radassController();
        const bool allForcedToHigh = !supportedItems.empty()
            && std::ranges::all_of(supportedItems,
                [this, controller](const auto& item)
                {
                    const auto widget =
                        qobject_cast<QnMediaResourceWidget*>(display()->widget(item.uuid()));
                    const auto camDisplay = widget ? widget->camDisplay() : nullptr;
                    return camDisplay && controller->effectiveMode(camDisplay) == RadassMode::High;
                });

        if (allForcedToHigh)
            effectiveMode = RadassMode::High;
    }

    auto addAction =
        [this, actionGroup, items, parent]
        (RadassMode mode, const QString& text, bool checked)
        {
            auto action = new QAction(parent);
            action->setText(text);
            action->setCheckable(true);
            action->setChecked(checked);
            connect(action, &QAction::triggered, this,
                [this, items, mode]
                {
                    Parameters parameters(items);
                    parameters.setArgument(Qn::ResolutionModeRole, (int)mode);
                    menu()->trigger(RadassAction, parameters);
                });
            actionGroup->addAction(action);
        };

    addAction(RadassMode::Auto, tr("Auto"), effectiveMode == RadassMode::Auto);
    addAction(RadassMode::Low, tr("Low"), effectiveMode == RadassMode::Low);
    addAction(RadassMode::High, tr("High"), effectiveMode == RadassMode::High);

    if (effectiveMode == RadassMode::Custom)
        addAction(RadassMode::Custom, tr("Custom"), true);

    return actionGroup->actions();
}

} // namespace menu
} // namespace nx::vms::client::desktop
