// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "radass_action_factory.h"

#include <QtWidgets/QAction>
#include <QtWidgets/QActionGroup>

#include <nx/vms/client/desktop/radass/radass_resource_manager.h>
#include <nx/vms/client/desktop/radass/radass_types.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>

namespace nx::vms::client::desktop {

RadassActionFactory::RadassActionFactory(QObject* parent):
    base_type(parent)
{
}

QList<QAction*> RadassActionFactory::newActions(const ui::action::Parameters& parameters,
    QObject* parent)
{
    auto actionGroup = new QActionGroup(parent);
    actionGroup->setExclusive(true);

    const auto manager = context()->instance<RadassResourceManager>();

    // If no layout items are provided, using current layout.
    auto items = parameters.layoutItems();
    const auto currentMode = items.empty()
        ? manager->mode(workbench()->currentLayoutResource())
        : manager->mode(items);

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
                    ui::action::Parameters parameters(items);
                    parameters.setArgument(Qn::ResolutionModeRole, (int)mode);
                    menu()->trigger(ui::action::RadassAction, parameters);
                });
            actionGroup->addAction(action);
        };

    addAction(RadassMode::Auto, tr("Auto"), currentMode == RadassMode::Auto);
    addAction(RadassMode::Low, tr("Low"), currentMode == RadassMode::Low);
    addAction(RadassMode::High, tr("High"), currentMode == RadassMode::High);

    if (currentMode == RadassMode::Custom)
        addAction(RadassMode::Custom, tr("Custom"), true);

    return actionGroup->actions();
}

} // namespace nx::vms::client::desktop
