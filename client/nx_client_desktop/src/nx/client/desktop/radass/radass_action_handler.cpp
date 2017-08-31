#include "radass_action_handler.h"

#include <QtWidgets/QAction>

#include <client/client_module.h>

#include <core/resource/layout_resource.h>
#include <core/resource/camera_resource.h>

#include <nx/client/desktop/radass/radass_types.h>
#include <nx/client/desktop/radass/radass_controller.h>
#include <nx/client/desktop/radass/radass_resource_manager.h>
#include <nx/client/desktop/radass/radass_cameras_watcher.h>

#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_manager.h>

#include <ui/graphics/items/resource/media_resource_widget.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>

namespace {

bool isValidWidget(QnMediaResourceWidget* widget)
{
    if (!widget || widget->isZoomWindow())
        return false;

    const auto camera = widget->resource()->toResourcePtr().dynamicCast<QnVirtualCameraResource>();
    return camera && camera->hasDualStreaming();
}

}

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

    connect(d->manager, &RadassResourceManager::modeChanged, this,
        &RadassActionHandler::handleItemModeChanged);

    connect(workbench(), &QnWorkbench::currentLayoutChanged, this,
        &RadassActionHandler::handleCurrentLayoutChanged);
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
    auto layoutItems = parameters.layoutItems();
    if (layoutItems.empty())
    {
        const auto layout = workbench()->currentLayout()->resource();
        QnLayoutItemIndexList items;
        for (const auto item: layout->getItems())
            items << QnLayoutItemIndex(layout, item.uuid);
    }

    QnLayoutItemIndexList validItems;
    for (const auto item: layoutItems)
    {
        auto widget = display()->widget(item.uuid());
        auto mediaWidget = qobject_cast<QnMediaResourceWidget*>(widget);
        if (isValidWidget(mediaWidget))
            validItems.push_back(item);
    }

    manager->setMode(validItems, mode);
}

void RadassActionHandler::handleItemModeChanged(const QnLayoutItemIndex& item, RadassMode mode)
{
    NX_ASSERT(item.layout());
    if (!item.layout())
        return;

    // Ignore items that do not belong to the current layout.
    if (item.layout() != workbench()->currentLayout()->resource())
        return;

    auto widget = display()->widget(item.uuid());
    auto mediaWidget = qobject_cast<QnMediaResourceWidget*>(widget);
    NX_ASSERT(isValidWidget(mediaWidget));
    if (isValidWidget(mediaWidget))
    {
        auto camDisplay = mediaWidget->display()->camDisplay();
        d->controller->setMode(camDisplay, mode);
    }
}

void RadassActionHandler::handleCurrentLayoutChanged()
{
    const auto layout = workbench()->currentLayout()->resource();
    if (!layout)
        return;

    for (const auto& item: workbench()->currentLayout()->items())
    {
        QnLayoutItemIndex index(layout, item->uuid());

        auto widget = display()->widget(index.uuid());
        auto mediaWidget = qobject_cast<QnMediaResourceWidget*>(widget);
        if (isValidWidget(mediaWidget))
        {
            auto camDisplay = mediaWidget->display()->camDisplay();
            d->controller->setMode(camDisplay, d->manager->mode(QnLayoutItemIndexList() << index));
        }
    }
}

} // namespace desktop
} // namespace client
} // namespace nx

