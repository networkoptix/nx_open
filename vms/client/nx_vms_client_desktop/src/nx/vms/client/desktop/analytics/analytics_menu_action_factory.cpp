#include "analytics_menu_action_factory.h"

#include <QtWidgets/QAction>
#include <QtWidgets/QActionGroup>

#include <core/resource/camera_resource.h>

#include <nx/vms/client/desktop/ui/actions/action_parameters.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>

#include <nx/vms/client/desktop/analytics/analytics_objects_visualization_manager.h>

#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>

namespace nx::vms::client::desktop {

namespace {

bool analyticsModeCanBeChangedForWidget(QnResourceWidget* widget)
{
    const auto mediaWidget = qobject_cast<QnMediaResourceWidget*>(widget);
    if (!mediaWidget)
        return false;

    if (!mediaWidget->isAnalyticsSupported())
        return false;

    const auto camera = widget->resource().dynamicCast<QnVirtualCameraResource>();
    return camera && !camera->enabledAnalyticsEngineResources().isEmpty();
}

} // namespace

AnalyticsMenuActionFactory::AnalyticsMenuActionFactory(QObject* parent):
    base_type(parent)
{
}

QList<QAction*> AnalyticsMenuActionFactory::newActions(const ui::action::Parameters& parameters,
    QObject* parent)
{
    const auto manager = context()->findInstance<AnalyticsObjectsVisualizationManager>();
    NX_ASSERT(manager);
    if (!manager)
        return {};

    QnLayoutItemIndexList items;
    auto widgets = parameters.widgets();
    for (const auto widget: widgets)
    {
        if (analyticsModeCanBeChangedForWidget(widget))
            items.push_back(ui::action::ParameterTypes::layoutItem(widget));
    }
    if (items.empty())
        return {};

    const auto currentMode = manager->mode(items);

    auto actionGroup = new QActionGroup(parent);
    actionGroup->setExclusive(true);

    const auto addAction =
        [this, actionGroup, items, parent]
        (AnalyticsObjectsVisualizationMode mode, const QString& text, bool checked)
        {
            auto action = new QAction(parent);
            action->setText(text);
            action->setCheckable(true);
            action->setChecked(checked);
            connect(action, &QAction::triggered, this,
                [this, items, mode]
                {
                    ui::action::Parameters parameters(items);
                    parameters.setArgument(Qn::AnalyticsObjectsVisualizationModeRole, (int)mode);
                    menu()->trigger(ui::action::AnalyticsObjectsVisualizationModeAction,
                        parameters);
                });
            actionGroup->addAction(action);
        };

    addAction(AnalyticsObjectsVisualizationMode::always, tr("Show Always"),
        currentMode == AnalyticsObjectsVisualizationMode::always);
    addAction(AnalyticsObjectsVisualizationMode::tabOnly, tr("Show Only with \"Objects\" Tab"),
        currentMode == AnalyticsObjectsVisualizationMode::tabOnly);

    return actionGroup->actions();
}

} // namespace nx::vms::client::desktop
