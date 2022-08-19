// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_tour_actions.h"

#include <QtWidgets/QAction>

#include <core/resource_management/layout_tour_manager.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/workbench/handlers/layout_tours_handler.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_state_manager.h>

namespace nx::vms::client::desktop {
namespace ui {
namespace action {

namespace
{

QnUuid tourId(const Parameters& parameters)
{
    return parameters.argument(Qn::UuidRole).value<QnUuid>();
}

bool tourIsRunning(QnWorkbenchContext* context)
{
    return context->action(ToggleLayoutTourModeAction)->isChecked();
}

class TourIsRunningCondition: public Condition
{
public:
    TourIsRunningCondition()
    {
    }

    virtual ActionVisibility check(const Parameters& /*parameters*/,
        QnWorkbenchContext* context) override
    {
        return tourIsRunning(context)
            ? EnabledAction
            : InvisibleAction;
    }
};

class CanToggleTourCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters,
        QnWorkbenchContext* context) override
    {
        const auto id = tourId(parameters);
        if (id.isNull())
        {
            if (context->workbench()->currentLayout()->items().size() > 1)
                return EnabledAction;
        }
        else
        {
            const auto tour = context->systemContext()->showreelManager()->tour(id);
            if (tour.isValid() && tour.items.size() > 0)
                return EnabledAction;
        }

        return DisabledAction;
    }
};

} // namespace

LayoutTourTextFactory::LayoutTourTextFactory(QObject* parent):
    base_type(parent)
{
}

QString LayoutTourTextFactory::text(const Parameters& parameters,
    QnWorkbenchContext* context) const
{
    if (tourIsRunning(context))
    {
        auto runningTourId = context->instance<workbench::LayoutToursHandler>()->runningTour();
        if (runningTourId.isNull())
            return tr("Stop Tour");
        return tr("Stop Showreel");
    }

    auto id = tourId(parameters);
    if (id.isNull())
    {
        // TODO: #sivanov Code duplication.
        const auto reviewTourId = context->workbench()->currentLayout()->data(
            Qn::LayoutTourUuidRole).value<QnUuid>();

        if (reviewTourId.isNull())
            return tr("Start Tour");
    }

    return tr("Start Showreel");
}

namespace condition
{

ConditionWrapper tourIsRunning()
{
    return new TourIsRunningCondition();
}

ConditionWrapper canStartTour()
{
    return new CanToggleTourCondition();
}

} // namespace condition

} // namespace action
} // namespace ui
} // namespace nx::vms::client::desktop
