#include "startup_actions_handler.h"

#include <chrono>

#include <QtCore/QScopedValueRollback>
#include <QtWidgets/QAction>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/layout_tour_manager.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>

#include <client/client_runtime_settings.h>
#include <client/client_startup_parameters.h>

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/utils/mime_data.h>

#include <ui/graphics/items/controls/time_slider.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_state_manager.h>
#include <ui/workbench/extensions/workbench_stream_synchronizer.h>
#include <ui/workbench/workbench_navigator.h>

#include <utils/common/synctime.h>

namespace {

using namespace std::chrono;
using namespace std::chrono_literals;

/** In ACS mode set timeline window to 5 minutes. */
static const milliseconds kAcsModeTimelineWindowSize = 5min;

}

namespace nx {
namespace client {
namespace desktop {

using namespace ui::action;

struct StartupActionsHandler::Private
{
    bool delayedDropGuard = false;

    // List of serialized resources that are to be dropped on the scene once the user logs in.
    struct
    {
        QByteArray raw;
        QList<QnUuid> resourceIds;
        qint64 timeStampMs = 0;

        bool empty() const
        {
            return raw.isEmpty() && resourceIds.empty();
        }
    } delayedDrops;
};

StartupActionsHandler::StartupActionsHandler(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d(new Private())
{
    connect(action(ProcessStartupParametersAction), &QAction::triggered, this,
        &StartupActionsHandler::handleStartupParameters);
    connect(context(), &QnWorkbenchContext::userChanged, this,
        &StartupActionsHandler::handleUserLoggedIn);
}

StartupActionsHandler::~StartupActionsHandler()
{
}

void StartupActionsHandler::handleUserLoggedIn(const QnUserResourcePtr& user)
{
    if (!user)
        return;

    // We should not change state when using "Open in New Window". Otherwise workbench will be
    // cleared here even if no state is saved.
    if (d->delayedDrops.empty() && qnRuntime->isDesktopMode())
        context()->instance<QnWorkbenchStateManager>()->restoreState();

    // Sometimes we get here when 'New Layout' has already been added. But all user's layouts must
    // be created AFTER this method. Otherwise the user will see uncreated layouts in layout
    // selection menu. As temporary workaround we can just remove that layouts.
    // TODO: #dklychkov Do not create new empty layout before this method end.
    // See: LayoutsHandler::at_openNewTabAction_triggered()
    if (user && !qnRuntime->isAcsMode())
    {
        for (const QnLayoutResourcePtr& layout: resourcePool()->getResourcesByParentId(
                 user->getId()).filtered<QnLayoutResource>())
        {
            if (layout->hasFlags(Qn::local) && !layout->isFile())
                resourcePool()->removeResource(layout);
        }
    }

    if (workbench()->layouts().empty())
        menu()->trigger(OpenNewTabAction);

    submitDelayedDrops();
}

void StartupActionsHandler::submitDelayedDrops()
{
    if (d->delayedDropGuard)
        return;

    if (d->delayedDrops.empty())
        return;

    // Delayed drops actual only for logged-in users.
    if (!context()->user())
        return;

    QScopedValueRollback<bool> guard(d->delayedDropGuard, true);

    QnResourceList resources;
    ec2::ApiLayoutTourDataList tours;

    /* Backport from 4.0 to simplify merge
    nx::vms::api::LayoutTourDataList tours;
    if (!m_delayedDropLayoutName.isEmpty())
    {
        if (const auto resource = getLayoutByName(m_delayedDropLayoutName, resourcePool()))
            resources.append(resource);
        else
            NX_ASSERT(false, "Wrong layout name");

        m_delayedDropLayoutName.clear();
    }
    */

    MimeData mimeData = MimeData::deserialized(d->delayedDrops.raw, resourcePool());

    for (const auto& tour: layoutTourManager()->tours(mimeData.entities()))
        tours.push_back(tour);

    resources.append(mimeData.resources());
    if (!resources.isEmpty())
        resourcePool()->addNewResources(resources);

    resources.append(resourcePool()->getResourcesByIds(d->delayedDrops.resourceIds));
    const auto timeStampMs = d->delayedDrops.timeStampMs;

    d->delayedDrops = {};

    if (resources.empty() && tours.empty())
        return;

    workbench()->clear();

    if (qnRuntime->isAcsMode())
    {
        handleAcsModeResources(resources, timeStampMs);
        return;
    }

    if (!resources.empty())
    {
        Parameters parameters(resources);
        if (timeStampMs != 0)
            parameters.setArgument(Qn::ItemTimeRole, timeStampMs);
        menu()->trigger(OpenInNewTabAction, parameters);
    }

    for (const auto& tour: tours)
        menu()->trigger(ReviewLayoutTourAction, {Qn::UuidRole, tour.id});
}

void StartupActionsHandler::handleStartupParameters()
{
    const auto data = menu()->currentParameters(sender())
        .argument<QnStartupParameters>(Qn::StartupParametersRole);

    if (!data.instantDrop.isEmpty())
    {
        const auto raw = QByteArray::fromBase64(data.instantDrop.toLatin1());
        MimeData mimeData = MimeData::deserialized(raw, resourcePool());
        const auto resources = mimeData.resources();
        resourcePool()->addNewResources(resources);
        handleInstantDrops(resources);
    }

    if (!data.delayedDrop.isEmpty())
    {
        d->delayedDrops.raw = QByteArray::fromBase64(data.instantDrop.toLatin1());
    }

    if (data.customUri.isValid())
    {
        d->delayedDrops.resourceIds = data.customUri.resourceIds();
        d->delayedDrops.timeStampMs = data.customUri.timestamp();
    }
    /* Backport from 4.0 to simplify merge
    else if (params.hasArgument(Qn::LayoutNameRole))
    {
        m_delayedDropLayoutName = params.argument<QString>(Qn::LayoutNameRole);
    }
    */

    submitDelayedDrops();
}

void StartupActionsHandler::handleInstantDrops(const QnResourceList& resources)
{
    if (resources.empty())
        return;

    workbench()->clear();
    const bool dropped = menu()->triggerIfPossible(OpenInNewTabAction, resources);
    if (dropped)
        action(ResourcesModeAction)->setChecked(true);

    // Security check - just in case.
    if (workbench()->layouts().empty())
        menu()->trigger(OpenNewTabAction);
}

void StartupActionsHandler::handleAcsModeResources(
    const QnResourceList& resources,
    const qint64 timeStampMs)
{
    const qint64 maxTime = qnSyncTime->currentMSecsSinceEpoch();

    QnTimePeriod period(
        timeStampMs - kAcsModeTimelineWindowSize.count() / 2,
        kAcsModeTimelineWindowSize.count());

    // Adjust period if start time was not passed or passed too near to live.
    if (period.startTimeMs < 0 || period.endTimeMs() > maxTime)
        period.startTimeMs = maxTime - kAcsModeTimelineWindowSize.count();

    QnLayoutResourcePtr layout(new QnLayoutResource());
    layout->setId(QnUuid::createUuid());
    layout->setParentId(context()->user()->getId());
    layout->setCellSpacing(0);
    resourcePool()->addResource(layout);

    const auto wlayout = new QnWorkbenchLayout(layout, this);
    workbench()->addLayout(wlayout);
    workbench()->setCurrentLayout(wlayout);

    menu()->trigger(OpenInCurrentLayoutAction, resources);

    // Set range to maximum allowed value, so we will not constrain window by any values.
    auto timeSlider = context()->navigator()->timeSlider();
    timeSlider->setRange(0, maxTime);
    timeSlider->setWindow(period.startTimeMs, period.endTimeMs(), false);

    navigator()->setPosition(timeStampMs * 1000);
}

} // namespace desktop
} // namespace client
} // namespace nx
