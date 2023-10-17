// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "showreel_actions.h"

#include <QtGui/QAction>

#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/menu/action.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/action_parameters.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/showreel/showreel_manager.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_state_manager.h>

#include "showreel_actions_handler.h"

namespace nx::vms::client::desktop {
namespace menu {

namespace
{

QnUuid showreelId(const Parameters& parameters)
{
    return parameters.argument(Qn::UuidRole).value<QnUuid>();
}

bool showreelIsRunning(WindowContext* context)
{
    return context->menu()->action(ToggleShowreelModeAction)->isChecked();
}

class ShowreelIsRunningCondition: public Condition
{
public:
    ShowreelIsRunningCondition()
    {
    }

    virtual ActionVisibility check(const Parameters& /*parameters*/,
        WindowContext* context) override
    {
        return showreelIsRunning(context)
            ? EnabledAction
            : InvisibleAction;
    }
};

class CanToggleShowreelCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters,
        WindowContext* context) override
    {
        const auto id = showreelId(parameters);
        if (id.isNull())
        {
            if (context->workbench()->currentLayout()->items().size() > 1)
                return EnabledAction;
        }
        else
        {
            const auto showreel = context->system()->showreelManager()->showreel(id);
            if (showreel.isValid() && showreel.items.size() > 0)
                return EnabledAction;
        }

        return DisabledAction;
    }
};

} // namespace

ShowreelTextFactory::ShowreelTextFactory(QObject* parent):
    base_type(parent)
{
}

QString ShowreelTextFactory::text(const Parameters& parameters, WindowContext* context) const
{
    if (showreelIsRunning(context))
    {
        auto runningTourId = context->showreelActionsHandler()->runningShowreel();
        if (runningTourId.isNull())
            return tr("Stop Tour");
        return tr("Stop Showreel");
    }

    auto id = showreelId(parameters);
    if (id.isNull())
    {
        // TODO: #sivanov Code duplication.
        const auto reviewTourId = context->workbench()->currentLayout()->data(
            Qn::ShowreelUuidRole).value<QnUuid>();

        if (reviewTourId.isNull())
            return tr("Start Tour");
    }

    return tr("Start Showreel");
}

namespace condition
{

ConditionWrapper showreelIsRunning()
{
    return new ShowreelIsRunningCondition();
}

ConditionWrapper canStartShowreel()
{
    return new CanToggleShowreelCondition();
}

} // namespace condition

} // namespace menu
} // namespace nx::vms::client::desktop
