// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "showreel_actions.h"

#include <QtGui/QAction>

#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/showreel/showreel_manager.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_state_manager.h>

#include "showreel_actions_handler.h"

namespace nx::vms::client::desktop {
namespace ui {
namespace action {

namespace
{

nx::Uuid showreelId(const Parameters& parameters)
{
    return parameters.argument(Qn::UuidRole).value<nx::Uuid>();
}

bool showreelIsRunning(QnWorkbenchContext* context)
{
    return context->action(ToggleShowreelModeAction)->isChecked();
}

class ShowreelIsRunningCondition: public Condition
{
public:
    ShowreelIsRunningCondition()
    {
    }

    virtual ActionVisibility check(const Parameters& /*parameters*/,
        QnWorkbenchContext* context) override
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
        QnWorkbenchContext* context) override
    {
        const auto id = showreelId(parameters);
        if (id.isNull())
        {
            if (context->workbench()->currentLayout()->items().size() > 1)
                return EnabledAction;
        }
        else
        {
            const auto showreel = context->systemContext()->showreelManager()->showreel(id);
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

QString ShowreelTextFactory::text(const Parameters& parameters,
    QnWorkbenchContext* context) const
{
    if (showreelIsRunning(context))
    {
        auto runningTourId = context->instance<ShowreelActionsHandler>()->runningShowreel();
        if (runningTourId.isNull())
            return tr("Stop Tour");
        return tr("Stop Showreel");
    }

    auto id = showreelId(parameters);
    if (id.isNull())
    {
        // TODO: #sivanov Code duplication.
        const auto reviewTourId = context->workbench()->currentLayout()->data(
            Qn::ShowreelUuidRole).value<nx::Uuid>();

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

} // namespace action
} // namespace ui
} // namespace nx::vms::client::desktop
