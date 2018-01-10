#include "action_conditions.h"

#include <QtWidgets/QAction>

#include <boost/algorithm/cxx11/all_of.hpp>
#include <boost/algorithm/cxx11/any_of.hpp>
#include <boost/range/algorithm/count_if.hpp>

#include <api/app_server_connection.h>

#include <common/common_module.h>

#include <network/router.h>

#include <core/resource_access/resource_access_subject.h>
#include <core/resource_access/resource_access_filter.h>
#include <core/resource_access/providers/resource_access_provider.h>
#include <core/resource_management/layout_tour_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource/fake_media_server.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/videowall_item_index.h>

#include <core/ptz/ptz_controller_pool.h>
#include <core/ptz/abstract_ptz_controller.h>
#include <recording/time_period_list.h>
#include <camera/resource_display.h>

#include <client/client_settings.h>

#include <network/cloud_url_validator.h>

#include <plugins/storage/file_storage/layout_storage_resource.h>

#include <recording/time_period.h>

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <ui/graphics/items/resource/button_ids.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/watchers/workbench_schedule_watcher.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/dialogs/ptz_manage_dialog.h>

#include <nx/vms/utils/platform/autorun.h>
#include <plugins/resource/desktop_camera/desktop_resource_base.h>

using boost::algorithm::any_of;
using boost::algorithm::all_of;

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace action {

namespace {

/**
 * Condition wich is a conjunction of two conditions.
 * It acts like logical AND, e.g. an action is enabled if the all conditions in the conjunction is true.
 * But the result (ActionVisibility) may have 3 values: [Invisible, Disabled, Enabled], so this action condition chooses
 * the minimal value from its conjuncts.
 */
class ConjunctionCondition: public Condition
{
public:
    ConjunctionCondition(ConditionWrapper&& l, ConditionWrapper&& r):
        l(std::move(l)),
        r(std::move(r))
    {
    }

    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override
    {
        // A bit of lazyness.
        auto first = l->check(parameters, context);
        if (first == InvisibleAction)
            return first;
        return qMin(first, r->check(parameters, context));
    }

private:
    ConditionWrapper l;
    ConditionWrapper r;
};

/**
 * Condition wich is a disjunction of two conditions.
 * It acts like logical OR, e.g. an action is enabled if one of conditions in the conjunction is true.
 * But the result (ActionVisibility) may have 3 values: [Invisible, Disabled, Enabled], so this action condition chooses
 * the maximal value from its conjuncts.
 */
class DisjunctionCondition: public Condition
{
public:
    DisjunctionCondition(ConditionWrapper&& l, ConditionWrapper&& r):
        l(std::move(l)),
        r(std::move(r))
    {
    }

    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override
    {
        // A bit of lazyness.
        auto first = l->check(parameters, context);
        if (first == EnabledAction)
            return first;
        return qMax(first, r->check(parameters, context));
    }

private:
    ConditionWrapper l;
    ConditionWrapper r;
};

class NegativeCondition: public Condition
{
public:
    NegativeCondition(ConditionWrapper&& condition):
        m_condition(std::move(condition))
    {
    }

    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override
    {
        return m_condition->check(parameters, context) == InvisibleAction
            ? EnabledAction
            : InvisibleAction;
    }

private:
    ConditionWrapper m_condition;
};

class ScopedCondition: public Condition
{
public:
    ScopedCondition(ActionScope scope, ConditionWrapper&& condition):
        m_scope(scope),
        m_condition(std::move(condition))
    {
    }

    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override
    {
        if (parameters.scope() != m_scope)
            return EnabledAction;
        return m_condition->check(parameters, context);
    }

private:
    ActionScope m_scope;
    ConditionWrapper m_condition;
};

class CustomBoolCondition: public Condition
{
public:
    using CheckDelegate = std::function<bool(
        const Parameters& parameters, QnWorkbenchContext* context)>;

    CustomBoolCondition(CheckDelegate delegate):
        m_delegate(delegate)
    {
    }

    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override
    {
        return m_delegate(parameters, context)
            ? EnabledAction
            : InvisibleAction;
    }

private:
    CheckDelegate m_delegate;
};

class CustomCondition: public Condition
{
public:
    using CheckDelegate = std::function<ActionVisibility(
        const Parameters& parameters, QnWorkbenchContext* context)>;

    CustomCondition(CheckDelegate delegate):
        m_delegate(delegate)
    {
    }

    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override
    {
        return m_delegate(parameters, context);
    }

private:
    CheckDelegate m_delegate;
};

class ResourceCondition: public Condition
{
public:
    using CheckDelegate = std::function<bool(const QnResourcePtr& resource)>;

    ResourceCondition(CheckDelegate delegate, MatchMode matchMode):
        m_delegate(delegate),
        m_matchMode(matchMode)
    {
    }

    ActionVisibility check(const QnResourceList& resources,
        QnWorkbenchContext* /*context*/)
    {
        return checkInternal<QnResourcePtr>(resources) ? EnabledAction : InvisibleAction;
    }

    ActionVisibility check(const QnResourceWidgetList& widgets,
        QnWorkbenchContext* /*context*/)
    {
        return checkInternal<QnResourceWidget*>(widgets) ? EnabledAction : InvisibleAction;
    }

     template<class Item, class ItemSequence>
     bool checkInternal(const ItemSequence &sequence)
     {
         int count = 0;

         for (const Item& item : sequence)
         {
             bool matches = checkOne(item);

             if (matches && m_matchMode == Any)
                 return true;

             if (!matches && m_matchMode == All)
                 return false;

             if (matches)
                 count++;
         }

         if (m_matchMode == Any)
             return false;

         if (m_matchMode == All)
             return true;

         if (m_matchMode == ExactlyOne)
             return count == 1;

         NX_EXPECT(false, lm("Invalid match mode '%1'.").arg(static_cast<int>(m_matchMode)));
         return false;
     }

     bool checkOne(const QnResourcePtr &resource)
     {
         return m_delegate(resource);
     }

     bool checkOne(QnResourceWidget* widget)
     {
         if (auto resource = ParameterTypes::resource(widget))
             return m_delegate(resource);
         return false;
     }

private:
    CheckDelegate m_delegate;
    MatchMode m_matchMode;
};

TimePeriodType periodType(const QnTimePeriod& period)
{
    if (period.isNull())
        return NullTimePeriod;

    if (period.isEmpty())
        return EmptyTimePeriod;

    return NormalTimePeriod;
}

} // namespace

Condition::Condition()
{
}

Condition::~Condition()
{
}

ActionVisibility Condition::check(const QnResourceList& /*resources*/, QnWorkbenchContext* /*context*/)
{
    return InvisibleAction;
}

ActionVisibility Condition::check(const QnLayoutItemIndexList& layoutItems, QnWorkbenchContext* context)
{
    return check(ParameterTypes::resources(layoutItems), context);
}

ActionVisibility Condition::check(const QnResourceWidgetList& widgets, QnWorkbenchContext* context)
{
    return check(ParameterTypes::layoutItems(widgets), context);
}

ActionVisibility Condition::check(const QnWorkbenchLayoutList& layouts, QnWorkbenchContext* context)
{
    return check(ParameterTypes::resources(layouts), context);
}

ActionVisibility Condition::check(const Parameters& parameters, QnWorkbenchContext* context)
{
    switch (parameters.type())
    {
        case ResourceType:
            return check(parameters.resources(), context);
        case WidgetType:
            return check(parameters.widgets(), context);
        case LayoutType:
            return check(parameters.layouts(), context);
        case LayoutItemType:
            return check(parameters.layoutItems(), context);
        default:
            NX_EXPECT(false, lm("Invalid parameter type '%1'.").arg(parameters.items().typeName()));
            return InvisibleAction;
    }
}

ConditionWrapper::ConditionWrapper()
{
}

ConditionWrapper::ConditionWrapper(Condition* condition):
    m_condition(condition)
{
}

Condition* ConditionWrapper::operator->() const
{
    return m_condition.get();
}

ConditionWrapper::operator bool() const
{
    return m_condition.get() != nullptr;
}

ConditionWrapper operator&&(ConditionWrapper&& l, ConditionWrapper&& r)
{
    return ConditionWrapper(new ConjunctionCondition(std::move(l), std::move(r)));
}

ConditionWrapper operator||(ConditionWrapper&& l, ConditionWrapper&& r)
{
    return ConditionWrapper(new DisjunctionCondition(std::move(l), std::move(r)));
}

ConditionWrapper operator!(ConditionWrapper&& l)
{
    return ConditionWrapper(new NegativeCondition(std::move(l)));
}

bool VideoWallReviewModeCondition::isVideoWallReviewMode(QnWorkbenchContext* context) const
{
    return context->workbench()->currentLayout()->data().contains(Qn::VideoWallResourceRole);
}

ActionVisibility VideoWallReviewModeCondition::check(const Parameters& /*parameters*/, QnWorkbenchContext* context)
{
    return isVideoWallReviewMode(context)
        ? EnabledAction
        : InvisibleAction;
}

ActionVisibility RequiresOwnerCondition::check(const Parameters& /*parameters*/, QnWorkbenchContext* context)
{
    if (context->user() && context->user()->isOwner())
        return EnabledAction;
    return InvisibleAction;
}

ItemZoomedCondition::ItemZoomedCondition(bool requiredZoomedState):
    m_requiredZoomedState(requiredZoomedState)
{
}

ActionVisibility ItemZoomedCondition::check(const QnResourceWidgetList& widgets, QnWorkbenchContext* context)
{
    if (widgets.size() != 1 || !widgets[0])
        return InvisibleAction;

    const auto widget = widgets[0];
    if (widget->resource()->hasFlags(Qn::videowall))
        return InvisibleAction;

    return ((widget->item() == context->workbench()->item(Qn::ZoomedRole)) == m_requiredZoomedState)
        ? EnabledAction
        : InvisibleAction;
}

SmartSearchCondition::SmartSearchCondition(bool requiredGridDisplayValue):
    m_hasRequiredGridDisplayValue(true),
    m_requiredGridDisplayValue(requiredGridDisplayValue)
{
}

SmartSearchCondition::SmartSearchCondition():
    m_hasRequiredGridDisplayValue(false),
    m_requiredGridDisplayValue(false)
{
}

ActionVisibility SmartSearchCondition::check(const QnResourceWidgetList& widgets, QnWorkbenchContext* /*context*/)
{
    auto pureIoModule = [](const QnResourcePtr& resource)
        {
            if (!resource->hasFlags(Qn::io_module))
                return false; //quick check

            QnMediaResourcePtr mediaResource = resource.dynamicCast<QnMediaResource>();
            return mediaResource && !mediaResource->hasVideo(0);
        };

    for (auto widget: widgets)
    {
        if (!widget)
            continue;

        if (!widget->resource()->hasFlags(Qn::motion))
            continue;

        if (widget->isZoomWindow())
            continue;

        if (pureIoModule(widget->resource()))
            continue;

        if (m_hasRequiredGridDisplayValue)
        {
            if (static_cast<bool>(widget->options() & QnResourceWidget::DisplayMotion) == m_requiredGridDisplayValue)
                return EnabledAction;
        }
        else
        {
            return EnabledAction;
        }
    }

    return InvisibleAction;
}

DisplayInfoCondition::DisplayInfoCondition(bool requiredDisplayInfoValue):
    m_hasRequiredDisplayInfoValue(true),
    m_requiredDisplayInfoValue(requiredDisplayInfoValue)
{
}

DisplayInfoCondition::DisplayInfoCondition():
    m_hasRequiredDisplayInfoValue(false),
    m_requiredDisplayInfoValue(false)
{
}

ActionVisibility DisplayInfoCondition::check(const QnResourceWidgetList& widgets, QnWorkbenchContext* /*context*/)
{
    for (auto widget: widgets)
    {
        if (!widget)
            continue;

        if (!(widget->visibleButtons() & Qn::InfoButton))
            continue;

        if (m_hasRequiredDisplayInfoValue)
        {
            if (static_cast<bool>(widget->options() & QnResourceWidget::DisplayInfo) == m_requiredDisplayInfoValue)
                return EnabledAction;
        }
        else
        {
            return EnabledAction;
        }
    }

    return InvisibleAction;
}

ActionVisibility ClearMotionSelectionCondition::check(const QnResourceWidgetList& widgets, QnWorkbenchContext* /*context*/)
{
    bool hasDisplayedGrid = false;

    for (auto widget: widgets)
    {
        if (!widget)
            continue;

        if (widget->options() & QnResourceWidget::DisplayMotion)
        {
            hasDisplayedGrid = true;

            if (auto mediaWidget = dynamic_cast<const QnMediaResourceWidget*>(widget))
                foreach(const QRegion &region, mediaWidget->motionSelection())
                if (!region.isEmpty())
                    return EnabledAction;
        }
    }

    return hasDisplayedGrid ? DisabledAction : InvisibleAction;
}

ActionVisibility ResourceRemovalCondition::check(const Parameters& parameters, QnWorkbenchContext* context)
{
    Qn::NodeType nodeType = parameters.argument<Qn::NodeType>(Qn::NodeTypeRole, Qn::ResourceNode);
    if (nodeType == Qn::SharedLayoutNode || nodeType == Qn::SharedResourceNode)
        return InvisibleAction;

    QnUserResourcePtr owner = parameters.argument<QnUserResourcePtr>(Qn::UserResourceRole);
    bool ownResources = owner && owner == context->user();

    auto canBeDeleted = [ownResources](const QnResourcePtr& resource)
    {
        NX_ASSERT(resource);
        if (!resource || !resource->resourcePool())
            return false;

        if (resource->hasFlags(Qn::layout) && !resource->hasFlags(Qn::local))
        {
            if (ownResources)
                return true;

            /* OK to remove autogenerated layout. */
            QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>();
            if (layout->isServiceLayout())
                return true;

            /* We cannot delete shared links on another user, they will be unshared instead. */
            if (layout->isShared())
                return false;

            return true;
        }

        if (resource->hasFlags(Qn::fake))
            return false;

        if (resource->hasFlags(Qn::remote_server))
            return resource->getStatus() == Qn::Offline;

        /* All other resources can be safely deleted if we have correct permissions. */
        return true;
    };

    return any_of(parameters.resources(), canBeDeleted)
        ? EnabledAction
        : InvisibleAction;
}

ActionVisibility StopSharingCondition::check(const Parameters& parameters, QnWorkbenchContext* context)
{
    if (context->commonModule()->isReadOnly())
        return InvisibleAction;

    Qn::NodeType nodeType = parameters.argument<Qn::NodeType>(Qn::NodeTypeRole, Qn::ResourceNode);
    if (nodeType != Qn::SharedLayoutNode)
        return InvisibleAction;

    auto user = parameters.argument<QnUserResourcePtr>(Qn::UserResourceRole);
    auto roleId = parameters.argument<QnUuid>(Qn::UuidRole);
    NX_ASSERT(user || !roleId.isNull());
    if (!user && roleId.isNull())
        return InvisibleAction;

    QnResourceAccessSubject subject = user
        ? QnResourceAccessSubject(user)
        : QnResourceAccessSubject(context->userRolesManager()->userRole(roleId));
    if (!subject.isValid())
        return InvisibleAction;

    for (auto resource : parameters.resources())
    {
        if (context->resourceAccessProvider()->accessibleVia(subject, resource) == nx::core::access::Source::shared)
            return EnabledAction;
    }

    return DisabledAction;
}

ActionVisibility RenameResourceCondition::check(const Parameters& parameters, QnWorkbenchContext* /*context*/)
{
    Qn::NodeType nodeType = parameters.argument<Qn::NodeType>(Qn::NodeTypeRole, Qn::ResourceNode);

    switch (nodeType)
    {
        case Qn::ResourceNode:
        case Qn::SharedLayoutNode:
        case Qn::SharedResourceNode:
        {
            if (parameters.resources().size() != 1)
                return InvisibleAction;

            QnResourcePtr target = parameters.resource();
            if (!target)
                return InvisibleAction;

            /* Renaming users directly from resource tree is disabled due do digest re-generation need. */
            if (target->hasFlags(Qn::user))
                return InvisibleAction;

            /* Edge servers renaming is forbidden. */
            if (QnMediaServerResource::isEdgeServer(target))
                return InvisibleAction;

            /* Incompatible resources cannot be renamed */
            if (target.dynamicCast<QnFakeMediaServerResource>())
                return InvisibleAction;

            return EnabledAction;
        }
        case Qn::EdgeNode:
        case Qn::RecorderNode:
            return EnabledAction;
        default:
            break;
    }

    return InvisibleAction;
}

ActionVisibility LayoutItemRemovalCondition::check(const QnLayoutItemIndexList& layoutItems, QnWorkbenchContext* context)
{
    foreach(const QnLayoutItemIndex &item, layoutItems)
        if (!context->accessController()->hasPermissions(item.layout(), Qn::WritePermission | Qn::AddRemoveItemsPermission))
            return InvisibleAction;

    return EnabledAction;
}

SaveLayoutCondition::SaveLayoutCondition(bool isCurrent):
    m_current(isCurrent)
{
}

ActionVisibility SaveLayoutCondition::check(const QnResourceList& resources, QnWorkbenchContext* context)
{
    QnLayoutResourcePtr layout;

    if (m_current)
    {
        layout = context->workbench()->currentLayout()->resource();
    }
    else
    {
        if (resources.size() != 1)
            return InvisibleAction;

        layout = resources[0].dynamicCast<QnLayoutResource>();
    }

    if (!layout)
        return InvisibleAction;

    if (layout->data().contains(Qn::VideoWallResourceRole))
        return InvisibleAction;

    if (layout->data().contains(Qn::LayoutTourUuidRole))
        return InvisibleAction;

    if (context->snapshotManager()->isSaveable(layout))
    {
        return EnabledAction;
    }
    else
    {
        return DisabledAction;
    }
}

SaveLayoutAsCondition::SaveLayoutAsCondition(bool isCurrent):
    m_current(isCurrent)
{
}

ActionVisibility SaveLayoutAsCondition::check(const QnResourceList& resources, QnWorkbenchContext* context)
{
    if (!context->user())
        return InvisibleAction;

    QnLayoutResourcePtr layout;
    if (m_current)
    {
        layout = context->workbench()->currentLayout()->resource();
    }
    else
    {
        if (resources.size() != 1)
            return InvisibleAction;

        layout = resources[0].dynamicCast<QnLayoutResource>();
    }

    if (!layout)
        return InvisibleAction;

    if (layout->isServiceLayout())
        return InvisibleAction;

    if (layout->data().contains(Qn::VideoWallResourceRole))
        return InvisibleAction;

    /* Save as.. for exported layouts works very strange, disabling it for now. */
    if (layout->isFile())
        return InvisibleAction;

    return EnabledAction;
}

LayoutCountCondition::LayoutCountCondition(int minimalRequiredCount):
    m_minimalRequiredCount(minimalRequiredCount)
{
}

ActionVisibility LayoutCountCondition::check(const Parameters& /*parameters*/, QnWorkbenchContext* context)
{
    if (context->workbench()->layouts().size() < m_minimalRequiredCount)
        return DisabledAction;
    return EnabledAction;
}

ActionVisibility TakeScreenshotCondition::check(const QnResourceWidgetList& widgets, QnWorkbenchContext* /*context*/)
{
    if (widgets.size() != 1)
        return InvisibleAction;

    auto widget = widgets[0];
    if (widget->resource()->flags() & (Qn::still_image | Qn::server))
        return InvisibleAction;

    Qn::RenderStatus renderStatus = widget->renderStatus();
    if (renderStatus == Qn::NothingRendered || renderStatus == Qn::CannotRender)
        return DisabledAction;

    return EnabledAction;
}

ActionVisibility AdjustVideoCondition::check(const QnResourceWidgetList& widgets, QnWorkbenchContext* /*context*/)
{
    if (widgets.size() != 1)
        return InvisibleAction;

    auto widget = widgets[0];
    if ((widget->resource()->flags() & (Qn::server | Qn::videowall | Qn::layout))
        || (widget->resource()->flags().testFlag(Qn::web_page)))
        return InvisibleAction;

    QString url = widget->resource()->getUrl().toLower();
    if ((widget->resource()->flags() & Qn::still_image) && !url.endsWith(lit(".jpg")) && !url.endsWith(lit(".jpeg")))
        return InvisibleAction;

    QnMediaResourcePtr mediaResource = widget->resource().dynamicCast<QnMediaResource>();
    if (mediaResource && !mediaResource->hasVideo(0))
        return InvisibleAction;

    Qn::RenderStatus renderStatus = widget->renderStatus();
    if (renderStatus == Qn::NothingRendered || renderStatus == Qn::CannotRender)
        return DisabledAction;

    return EnabledAction;
}

TimePeriodCondition::TimePeriodCondition(TimePeriodTypes periodTypes, ActionVisibility nonMatchingVisibility):
    m_periodTypes(periodTypes),
    m_nonMatchingVisibility(nonMatchingVisibility)
{
}

ActionVisibility TimePeriodCondition::check(const Parameters& parameters, QnWorkbenchContext* /*context*/)
{
    if (!parameters.hasArgument(Qn::TimePeriodRole))
        return InvisibleAction;

    QnTimePeriod period = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);
    if (m_periodTypes.testFlag(periodType(period)))
        return EnabledAction;

    return m_nonMatchingVisibility;
}

ExportCondition::ExportCondition(bool centralItemRequired):
    m_centralItemRequired(centralItemRequired)
{
}

ActionVisibility ExportCondition::check(const Parameters& parameters, QnWorkbenchContext* context)
{
    if (!parameters.hasArgument(Qn::TimePeriodRole))
        return InvisibleAction;

    QnTimePeriod period = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);
    if (periodType(period) != NormalTimePeriod)
        return DisabledAction;

    // Export selection
    if (m_centralItemRequired)
    {

        const auto containsAvailablePeriods = parameters.hasArgument(Qn::TimePeriodsRole);

        /// If parameters contain periods it means we need current selected item
        if (containsAvailablePeriods && !context->workbench()->item(Qn::CentralRole))
            return DisabledAction;

        QnResourcePtr resource = parameters.resource();
        if (containsAvailablePeriods && resource && resource->flags().testFlag(Qn::sync))
        {
            QnTimePeriodList periods = parameters.argument<QnTimePeriodList>(Qn::TimePeriodsRole);
            if (!periods.intersects(period))
                return DisabledAction;
        }
    }
    // Export layout
    else
    {
        QnTimePeriodList periods = parameters.argument<QnTimePeriodList>(Qn::MergedTimePeriodsRole);
        if (!periods.intersects(period))
            return DisabledAction;
    }
    return EnabledAction;
}

ActionVisibility AddBookmarkCondition::check(const Parameters& parameters, QnWorkbenchContext* context)
{
    if (!parameters.hasArgument(Qn::TimePeriodRole))
        return InvisibleAction;

    QnTimePeriod period = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);
    if (periodType(period) != NormalTimePeriod)
        return DisabledAction;

    if (!context->workbench()->item(Qn::CentralRole))
        return DisabledAction;

    QnResourcePtr resource = parameters.resource();
    if (!resource->flags().testFlag(Qn::live))
        return InvisibleAction;

    if (resource->flags().testFlag(Qn::sync))
    {
        QnTimePeriodList periods = parameters.argument<QnTimePeriodList>(Qn::TimePeriodsRole);
        if (!periods.intersects(period))
            return DisabledAction;
    }

    return EnabledAction;
}

ActionVisibility ModifyBookmarkCondition::check(const Parameters& parameters, QnWorkbenchContext* /*context*/)
{
    if (!parameters.hasArgument(Qn::CameraBookmarkRole))
        return InvisibleAction;
    return EnabledAction;
}

ActionVisibility RemoveBookmarksCondition::check(const Parameters& parameters, QnWorkbenchContext* /*context*/)
{
    if (!parameters.hasArgument(Qn::CameraBookmarkListRole))
        return InvisibleAction;

    if (parameters.argument(Qn::CameraBookmarkListRole).value<QnCameraBookmarkList>().isEmpty())
        return InvisibleAction;

    return EnabledAction;
}

PreviewCondition::PreviewCondition():
    ExportCondition(true)
{
}

ActionVisibility PreviewCondition::check(const Parameters& parameters, QnWorkbenchContext* context)
{
    QnMediaResourcePtr media = parameters.resource().dynamicCast<QnMediaResource>();
    if (!media)
        return InvisibleAction;

    bool isImage = media->toResource()->flags() & Qn::still_image;
    if (isImage)
        return InvisibleAction;

    bool isPanoramic = media->getVideoLayout(0)->channelCount() > 1;
    if (isPanoramic)
        return InvisibleAction;

    if (context->workbench()->currentLayout()->isSearchLayout())
        return EnabledAction;

    return ExportCondition::check(parameters, context);
}

ActionVisibility PanicCondition::check(const Parameters& /*parameters*/, QnWorkbenchContext* context)
{
    return context->instance<QnWorkbenchScheduleWatcher>()->isScheduleEnabled() ? EnabledAction : DisabledAction;
}

ActionVisibility StartCurrentLayoutTourCondition::check(const Parameters& /*parameters*/, QnWorkbenchContext* context)
{
    const auto tourId = context->workbench()->currentLayout()->data()
        .value(Qn::LayoutTourUuidRole).value<QnUuid>();
    const auto tour = context->layoutTourManager()->tour(tourId);
    if (tour.isValid() && tour.items.size() > 0)
        return EnabledAction;
    return DisabledAction;
}

ActionVisibility ArchiveCondition::check(const QnResourceList& resources, QnWorkbenchContext* /*context*/)
{
    if (resources.size() != 1)
        return InvisibleAction;

    bool hasFootage = resources[0]->flags().testFlag(Qn::video);
    return hasFootage
        ? EnabledAction
        : InvisibleAction;
}

ActionVisibility TimelineVisibleCondition::check(const Parameters& /*parameters*/, QnWorkbenchContext* context)
{
    return context->navigator()->isPlayingSupported()
        ? EnabledAction
        : InvisibleAction;
}

ActionVisibility ToggleTitleBarCondition::check(const Parameters& /*parameters*/, QnWorkbenchContext* context)
{
    return context->action(action::EffectiveMaximizeAction)->isChecked() ? EnabledAction : InvisibleAction;
}

ActionVisibility NoArchiveCondition::check(const Parameters& /*parameters*/, QnWorkbenchContext* context)
{
    return context->accessController()->hasGlobalPermission(Qn::GlobalViewArchivePermission)
        ? InvisibleAction
        : EnabledAction;
}

ActionVisibility OpenInFolderCondition::check(const QnResourceList& resources, QnWorkbenchContext* /*context*/)
{
    if (resources.size() != 1)
        return InvisibleAction;

    QnResourcePtr resource = resources[0];

    // Skip cameras inside the exported layouts.
    bool isLocalResource = resource->hasFlags(Qn::local_media) && resource->getParentId().isNull();

    bool isExportedLayout = resource->hasFlags(Qn::exported_layout);

    return isLocalResource || isExportedLayout ? EnabledAction : InvisibleAction;
}

ActionVisibility OpenInFolderCondition::check(const QnLayoutItemIndexList& layoutItems, QnWorkbenchContext* context)
{
    foreach(const QnLayoutItemIndex &index, layoutItems)
    {
        QnLayoutItemData itemData = index.layout()->getItem(index.uuid());
        if (itemData.zoomRect.isNull())
            return Condition::check(layoutItems, context);
    }

    return InvisibleAction;
}

ActionVisibility LayoutSettingsCondition::check(const QnResourceList& resources, QnWorkbenchContext* context)
{
    if (resources.size() > 1)
        return InvisibleAction;

    QnResourcePtr resource;
    if (resources.size() > 0)
        resource = resources[0];
    else
        resource = context->workbench()->currentLayout()->resource();

    if (!resource)
        return InvisibleAction;

    if (!context->accessController()->hasPermissions(resource, Qn::EditLayoutSettingsPermission))
        return InvisibleAction;
    return EnabledAction;
}

ActionVisibility CreateZoomWindowCondition::check(const QnResourceWidgetList& widgets, QnWorkbenchContext* context)
{
    if (widgets.size() != 1)
        return InvisibleAction;

    // TODO: #Elric there probably exists a better way to check it all.

    auto widget = dynamic_cast<QnMediaResourceWidget*>(widgets[0]);
    if (!widget)
        return InvisibleAction;

    if (context->display()->zoomTargetWidget(widget))
        return InvisibleAction;

    return EnabledAction;
}

ActionVisibility NewUserLayoutCondition::check(const Parameters& parameters, QnWorkbenchContext* context)
{
    if (!parameters.hasArgument(Qn::NodeTypeRole))
        return InvisibleAction;

    const auto nodeType = parameters.argument(Qn::NodeTypeRole).value<Qn::NodeType>();
    const auto user = parameters.hasArgument(Qn::UserResourceRole)
        ? parameters.argument(Qn::UserResourceRole).value<QnUserResourcePtr>()
        : parameters.resource().dynamicCast<QnUserResource>();

    /* Create layout for self. */
    if (nodeType == Qn::LayoutsNode)
        return EnabledAction;

    // No other nodes must provide a way to create own layout.
    if (!user || user == context->user())
        return InvisibleAction;

    // Create layout for the custom user, but not for role.
    if (nodeType == Qn::SharedLayoutsNode && user)
        return EnabledAction;

    // Create layout for other user is allowed on this user's node.
    if (nodeType != Qn::ResourceNode)
        return InvisibleAction;

    return context->accessController()->canCreateLayout(user->getId())
        ? EnabledAction
        : InvisibleAction;
}

ActionVisibility OpenInLayoutCondition::check(const Parameters& parameters, QnWorkbenchContext* /*context*/)
{
    auto layout = parameters.argument<QnLayoutResourcePtr>(Qn::LayoutResourceRole);
    if (!layout)
        return InvisibleAction;

    return canOpen(parameters.resources(), layout)
        ? EnabledAction
        : InvisibleAction;
}

bool OpenInLayoutCondition::canOpen(const QnResourceList& resources,
    const QnLayoutResourcePtr& layout) const
{
    if (!layout)
        return any_of(resources, QnResourceAccessFilter::isOpenableInEntity);

    bool isExportedLayout = layout->isFile();

    for (const auto& resource: resources)
    {
        if (!QnResourceAccessFilter::isOpenableInLayout(resource))
            continue;

        /* Allow to duplicate items on the exported layout. */
        if (isExportedLayout && resource->getParentId() == layout->getId())
            return true;

        bool nonMedia = !resource->hasFlags(Qn::media);
        bool isLocalResource = resource->hasFlags(Qn::local_media);

        bool forbidden = isExportedLayout && (nonMedia || isLocalResource);
        if (!forbidden)
            return true;
    }

    return false;
}

ActionVisibility OpenInCurrentLayoutCondition::check(const QnResourceList& resources, QnWorkbenchContext* context)
{
    QnLayoutResourcePtr layout = context->workbench()->currentLayout()->resource();

    if (!layout)
        return InvisibleAction;

    return canOpen(resources, layout)
        ? EnabledAction
        : InvisibleAction;
}

ActionVisibility OpenInCurrentLayoutCondition::check(const Parameters& parameters, QnWorkbenchContext* context)
{
    /* Make sure we will get to specialized implementation */
    return Condition::check(parameters, context);
}

ActionVisibility OpenInNewEntityCondition::check(const QnResourceList& resources, QnWorkbenchContext* /*context*/)
{
    return canOpen(resources, QnLayoutResourcePtr())
        ? EnabledAction
        : InvisibleAction;
}

ActionVisibility OpenInNewEntityCondition::check(const QnLayoutItemIndexList& layoutItems, QnWorkbenchContext* context)
{
    foreach(const QnLayoutItemIndex &index, layoutItems)
    {
        QnLayoutItemData itemData = index.layout()->getItem(index.uuid());
        if (itemData.zoomRect.isNull())
            return Condition::check(layoutItems, context);
    }

    return InvisibleAction;
}

ActionVisibility OpenInNewEntityCondition::check(const Parameters& parameters, QnWorkbenchContext* context)
{
    /* Make sure we will get to specialized implementation */
    return Condition::check(parameters, context);
}

ActionVisibility SetAsBackgroundCondition::check(const QnResourceList& resources, QnWorkbenchContext* context)
{
    if (resources.size() != 1)
        return InvisibleAction;
    QnResourcePtr resource = resources[0];
    if (!resource->hasFlags(Qn::url | Qn::local | Qn::still_image))
        return InvisibleAction;

    QnLayoutResourcePtr layout = context->workbench()->currentLayout()->resource();
    if (!layout)
        return InvisibleAction;

    if (layout->locked())
        return DisabledAction;
    return EnabledAction;
}

ActionVisibility SetAsBackgroundCondition::check(const QnLayoutItemIndexList& layoutItems, QnWorkbenchContext* context)
{
    foreach(const QnLayoutItemIndex &index, layoutItems)
    {
        QnLayoutItemData itemData = index.layout()->getItem(index.uuid());
        if (itemData.zoomRect.isNull())
            return Condition::check(layoutItems, context);
    }

    return InvisibleAction;
}

ActionVisibility BrowseLocalFilesCondition::check(const Parameters& /*parameters*/, QnWorkbenchContext* context)
{
    const bool connected = !context->commonModule()->remoteGUID().isNull();
    return (connected ? InvisibleAction : EnabledAction);
}

ActionVisibility ChangeResolutionCondition::check(const Parameters& /*parameters*/, QnWorkbenchContext* context)
{
    if (isVideoWallReviewMode(context))
        return InvisibleAction;

    if (!context->user())
        return InvisibleAction;

    QnLayoutResourcePtr layout = context->workbench()->currentLayout()->resource();
    if (!layout)
        return InvisibleAction;

    if (layout->isFile())
        return InvisibleAction;

    return EnabledAction;
}

PtzCondition::PtzCondition(Ptz::Capabilities capabilities, bool disableIfPtzDialogVisible):
    m_capabilities(capabilities),
    m_disableIfPtzDialogVisible(disableIfPtzDialogVisible)
{
}

ActionVisibility PtzCondition::check(const Parameters& parameters, QnWorkbenchContext* context)
{
    bool isPreviewSearchMode =
        parameters.scope() == SceneScope &&
        context->workbench()->currentLayout()->isSearchLayout();
    if (isPreviewSearchMode)
        return InvisibleAction;
    return Condition::check(parameters, context);
}

ActionVisibility PtzCondition::check(const QnResourceList& resources, QnWorkbenchContext* /*context*/)
{
    foreach(const QnResourcePtr &resource, resources)
        if (!check(qnPtzPool->controller(resource)))
            return InvisibleAction;

    if (m_disableIfPtzDialogVisible && QnPtzManageDialog::instance() && QnPtzManageDialog::instance()->isVisible())
        return DisabledAction;

    return EnabledAction;
}

ActionVisibility PtzCondition::check(const QnResourceWidgetList& widgets, QnWorkbenchContext* /*context*/)
{
    for (auto widget: widgets)
    {
        auto mediaWidget = dynamic_cast<const QnMediaResourceWidget*>(widget);
        if (!mediaWidget)
            return InvisibleAction;

        if (!check(mediaWidget->ptzController()))
            return InvisibleAction;

        if (mediaWidget->isZoomWindow())
            return InvisibleAction;
    }

    if (m_disableIfPtzDialogVisible && QnPtzManageDialog::instance() && QnPtzManageDialog::instance()->isVisible())
        return DisabledAction;

    return EnabledAction;
}

bool PtzCondition::check(const QnPtzControllerPtr &controller)
{
    return controller && controller->hasCapabilities(m_capabilities);
}

ActionVisibility NonEmptyVideowallCondition::check(const QnResourceList& resources, QnWorkbenchContext* /*context*/)
{
    foreach(const QnResourcePtr &resource, resources)
    {
        if (!resource->hasFlags(Qn::videowall))
            continue;

        QnVideoWallResourcePtr videowall = resource.dynamicCast<QnVideoWallResource>();
        if (!videowall)
            continue;

        if (videowall->items()->getItems().isEmpty())
            continue;

        return EnabledAction;
    }
    return InvisibleAction;
}

SaveVideowallReviewCondition::SaveVideowallReviewCondition(bool isCurrent):
    m_current(isCurrent)
{
}

ActionVisibility SaveVideowallReviewCondition::check(const QnResourceList& resources, QnWorkbenchContext* context)
{
    QnLayoutResourceList layouts;

    if (m_current)
    {
        auto layout = context->workbench()->currentLayout()->resource();
        if (!layout)
            return InvisibleAction;

        if (context->workbench()->currentLayout()->data().contains(Qn::VideoWallResourceRole))
            layouts << layout;
    }
    else
    {
        foreach(const QnResourcePtr &resource, resources)
        {
            if (!resource->hasFlags(Qn::videowall))
                continue;

            QnVideoWallResourcePtr videowall = resource.dynamicCast<QnVideoWallResource>();
            if (!videowall)
                continue;

            QnWorkbenchLayout* layout = QnWorkbenchLayout::instance(videowall);
            if (!layout)
                continue;
            layouts << layout->resource();
        }
    }

    if (layouts.isEmpty())
        return InvisibleAction;

    foreach(const QnLayoutResourcePtr &layout, layouts)
        if (context->snapshotManager()->isModified(layout))
            return EnabledAction;

    return DisabledAction;
}

ActionVisibility RunningVideowallCondition::check(const QnResourceList& resources, QnWorkbenchContext* /*context*/)
{
    bool hasNonEmptyVideowall = false;
    foreach(const QnResourcePtr &resource, resources)
    {
        if (!resource->hasFlags(Qn::videowall))
            continue;

        QnVideoWallResourcePtr videowall = resource.dynamicCast<QnVideoWallResource>();
        if (!videowall)
            continue;

        if (videowall->items()->getItems().isEmpty())
            continue;

        hasNonEmptyVideowall = true;
        if (videowall->onlineItems().isEmpty())
            continue;

        return EnabledAction;
    }

    return hasNonEmptyVideowall
        ? DisabledAction
        : InvisibleAction;
}

ActionVisibility StartVideowallCondition::check(const QnResourceList& resources, QnWorkbenchContext* /*context*/)
{
    QnUuid pcUuid = qnSettings->pcUuid();
    if (pcUuid.isNull())
        return InvisibleAction;

    bool hasAttachedItems = false;
    foreach(const QnResourcePtr &resource, resources)
    {
        if (!resource->hasFlags(Qn::videowall))
            continue;

        QnVideoWallResourcePtr videowall = resource.dynamicCast<QnVideoWallResource>();
        if (!videowall)
            continue;

        if (!videowall->pcs()->hasItem(pcUuid))
            continue;

        foreach(const QnVideoWallItem &item, videowall->items()->getItems())
        {
            if (item.pcUuid != pcUuid)
                continue;

            if (!item.runtimeStatus.online)
                return EnabledAction;

            hasAttachedItems = true;
        }
    }

    return hasAttachedItems
        ? DisabledAction
        : InvisibleAction;
}

ActionVisibility IdentifyVideoWallCondition::check(const Parameters& parameters, QnWorkbenchContext* context)
{
    if (parameters.videoWallItems().size() > 0)
    {
        // allow action if there is at least one online item
        foreach(const QnVideoWallItemIndex &index, parameters.videoWallItems())
        {
            if (!index.isValid())
                continue;
            if (index.item().runtimeStatus.online)
                return EnabledAction;
        }
        return InvisibleAction;
    }

    /* 'Identify' action should not be displayed as disabled anyway. */
    ActionVisibility baseResult = Condition::check(parameters, context);
    if (baseResult != EnabledAction)
        return InvisibleAction;
    return EnabledAction;
}

ActionVisibility DetachFromVideoWallCondition::check(const Parameters& parameters, QnWorkbenchContext* context)
{
    if (!context->user() || parameters.videoWallItems().isEmpty())
        return InvisibleAction;

    if (context->commonModule()->isReadOnly())
        return InvisibleAction;

    foreach(const QnVideoWallItemIndex &index, parameters.videoWallItems())
    {
        if (index.isNull())
            continue;
        if (!index.videowall()->items()->hasItem(index.uuid()))
            continue;
        if (index.videowall()->items()->getItem(index.uuid()).layout.isNull())
            continue;
        return EnabledAction;
    }
    return InvisibleAction;
}

ActionVisibility StartVideoWallControlCondition::check(const Parameters& parameters, QnWorkbenchContext* context)
{
    if (!context->user() || parameters.videoWallItems().isEmpty())
        return InvisibleAction;

    foreach(const QnVideoWallItemIndex &index, parameters.videoWallItems())
    {
        if (!index.isValid())
            continue;

        return EnabledAction;
    }
    return InvisibleAction;
}

ActionVisibility RotateItemCondition::check(const QnResourceWidgetList& widgets, QnWorkbenchContext* /*context*/)
{
    for (auto widget: widgets)
    {
        if (widget->options() & QnResourceWidget::WindowRotationForbidden)
            return InvisibleAction;
    }
    return EnabledAction;
}

LightModeCondition::LightModeCondition(Qn::LightModeFlags flags):
    m_lightModeFlags(flags)
{
}

ActionVisibility LightModeCondition::check(const Parameters& /*parameters*/, QnWorkbenchContext* /*context*/)
{
    if (qnSettings->lightMode() & m_lightModeFlags)
        return InvisibleAction;
    return EnabledAction;
}

EdgeServerCondition::EdgeServerCondition(bool isEdgeServer):
    m_isEdgeServer(isEdgeServer)
{
}

ActionVisibility EdgeServerCondition::check(const QnResourceList& resources, QnWorkbenchContext* /*context*/)
{
    foreach(const QnResourcePtr &resource, resources)
        if (m_isEdgeServer != QnMediaServerResource::isEdgeServer(resource))
            return InvisibleAction;
    return EnabledAction;
}

ResourceStatusCondition::ResourceStatusCondition(const QSet<Qn::ResourceStatus> statuses, bool allResources):
    m_statuses(statuses),
    m_all(allResources)
{
}

ResourceStatusCondition::ResourceStatusCondition(Qn::ResourceStatus status, bool allResources):
    ResourceStatusCondition({{status}}, allResources)
{
}

ActionVisibility ResourceStatusCondition::check(const QnResourceList& resources, QnWorkbenchContext* /*context*/)
{
    bool found = false;
    foreach(const QnResourcePtr &resource, resources)
    {
        if (!m_statuses.contains(resource->getStatus()))
        {
            if (m_all)
                return InvisibleAction;
        }
        else
        {
            found = true;
        }
    }
    return found ? EnabledAction : InvisibleAction;
}

ActionVisibility DesktopCameraCondition::check(const Parameters& /*parameters*/,
    QnWorkbenchContext* context)
{
    const auto screenRecordingAction = context->action(action::ToggleScreenRecordingAction);
    if (screenRecordingAction)
    {
        const auto user = context->user();
        if (!user)
            return InvisibleAction;

        const auto desktopCameraId = QnDesktopResource::calculateUniqueId(
            context->commonModule()->moduleGUID(), user->getId());

        /* Do not check real pointer type to speed up check. */
        const auto desktopCamera = context->resourcePool()->getResourceByUniqueId(
            desktopCameraId);
#ifdef DESKTOP_CAMERA_DEBUG
        NX_ASSERT(!desktopCamera || (desktopCamera->hasFlags(Qn::desktop_camera) && desktopCamera->getParentId() == commonModule()->remoteGUID()),
            Q_FUNC_INFO,
            "Desktop camera must have correct flags and parent (if exists)");
#endif
        if (desktopCamera && desktopCamera->hasFlags(Qn::desktop_camera))
            return EnabledAction;

        return DisabledAction;
    }

    // Screen recording is not supported
    return InvisibleAction;
}

ActionVisibility AutoStartAllowedCondition::check(const Parameters& /*parameters*/, QnWorkbenchContext* /*context*/)
{
    if (!nx::vms::utils::isAutoRunSupported())
        return InvisibleAction;
    return EnabledAction;
}

ItemsCountCondition::ItemsCountCondition(int count):
    m_count(count)
{
}

ActionVisibility ItemsCountCondition::check(const Parameters& /*parameters*/, QnWorkbenchContext* context)
{
    int count = context->workbench()->currentLayout()->items().size();

    return (m_count == MultipleItems && count > 1) || (m_count == count) ? EnabledAction : InvisibleAction;
}

ActionVisibility IoModuleCondition::check(const QnResourceList& resources, QnWorkbenchContext* /*context*/)
{
    bool pureIoModules = boost::algorithm::all_of(resources,
        [](const QnResourcePtr& resource)
        {
            if (!resource->hasFlags(Qn::io_module))
                return false; //quick check

            QnMediaResourcePtr mediaResource = resource.dynamicCast<QnMediaResource>();
            return mediaResource && !mediaResource->hasVideo(0);
        });

    return pureIoModules ? EnabledAction : InvisibleAction;
}

ActionVisibility MergeToCurrentSystemCondition::check(const QnResourceList& resources, QnWorkbenchContext* /*context*/)
{
    if (resources.size() != 1)
        return InvisibleAction;

    QnMediaServerResourcePtr server = resources.first().dynamicCast<QnMediaServerResource>();
    if (!server)
        return InvisibleAction;

    Qn::ResourceStatus status = server->getStatus();
    if (status != Qn::Incompatible && status != Qn::Unauthorized)
        return InvisibleAction;

    if (server->getModuleInformation().ecDbReadOnly)
        return InvisibleAction;

    return EnabledAction;
}


FakeServerCondition::FakeServerCondition(bool allResources):
    m_all(allResources)
{
}

ActionVisibility FakeServerCondition::check(const QnResourceList& resources, QnWorkbenchContext* /*context*/)
{
    bool found = false;
    foreach(const QnResourcePtr &resource, resources)
    {
        QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;

        if (server.dynamicCast<QnFakeMediaServerResource>())
            found = true;
        else if (m_all)
            return InvisibleAction;
    }
    return found ? EnabledAction : InvisibleAction;
}

CloudServerCondition::CloudServerCondition(MatchMode matchMode):
    m_matchMode(matchMode)
{
}

ActionVisibility CloudServerCondition::check(const QnResourceList& resources, QnWorkbenchContext* /*context*/)
{
    auto isCloudServer = [](const QnResourcePtr& resource)
        {
            return nx::network::isCloudServer(resource.dynamicCast<QnMediaServerResource>());
        };

    bool success = false;
    switch (m_matchMode)
    {
        case Any:
            success = any_of(resources, isCloudServer);
            break;
        case All:
            success = all_of(resources, isCloudServer);
            break;
        case ExactlyOne:
            success = (boost::count_if(resources, isCloudServer) == 1);
            break;
        default:
            break;
    }

    return success ? EnabledAction : InvisibleAction;
}

namespace condition {

ConditionWrapper always()
{
    return new CustomBoolCondition(
        [](const Parameters& /*parameters*/, QnWorkbenchContext* /*context*/)
        {
            return true;
        });
}

ConditionWrapper isTrue(bool value)
{
    return new CustomBoolCondition(
        [value](const Parameters& /*parameters*/, QnWorkbenchContext* /*context*/)
        {
            return value;
        });
}

ConditionWrapper isLoggedIn()
{
    return new CustomBoolCondition(
        [](const Parameters& /*parameters*/, QnWorkbenchContext* context)
        {
            return !context->commonModule()->remoteGUID().isNull();
        });
}

ConditionWrapper scoped(ActionScope scope, ConditionWrapper&& condition)
{
    return new ScopedCondition(scope, std::move(condition));
}

ConditionWrapper isPreviewSearchMode()
{
    return new CustomBoolCondition(
        [](const Parameters& parameters, QnWorkbenchContext* context)
        {
            return parameters.scope() == SceneScope
                && context->workbench()->currentLayout()->isSearchLayout();
        });
}

ConditionWrapper isSafeMode()
{
    return new CustomBoolCondition(
        [](const Parameters& /*parameters*/, QnWorkbenchContext* context)
        {
            return context->commonModule()->isReadOnly();
        });
}

ConditionWrapper hasFlags(Qn::ResourceFlags flags, MatchMode matchMode)
{
    return new ResourceCondition(
        [flags](const QnResourcePtr& resource)
        {
            return resource->hasFlags(flags);
        }, matchMode);
}

ConditionWrapper isWearable(MatchMode matchMode) 
{
    return new ResourceCondition(
        [](const QnResourcePtr& resource)
        {
            return resource->getTypeId() == QnResourceTypePool::kWearableCameraTypeUuid;
        }, 
        matchMode);
}

ConditionWrapper treeNodeType(QSet<Qn::NodeType> types)
{
    return new CustomBoolCondition(
        [types](const Parameters& parameters, QnWorkbenchContext* /*context*/)
        {
            // Actions, triggered manually or from scene, must not check node type
            return !parameters.hasArgument(Qn::NodeTypeRole)
                || types.contains(parameters.argument(Qn::NodeTypeRole).value<Qn::NodeType>());
        });
}

ConditionWrapper isLayoutTourReviewMode()
{
    return new CustomBoolCondition(
        [](const Parameters& /*parameters*/, QnWorkbenchContext* context)
        {
            return context->workbench()->currentLayout()->isLayoutTourReview();
        });
}

ConditionWrapper canSavePtzPosition()
{
    return new CustomBoolCondition(
        [](const Parameters& parameters, QnWorkbenchContext* /*context*/)
        {
            auto widget = qobject_cast<const QnMediaResourceWidget*>(parameters.widget());
            NX_EXPECT(widget);
            if (!widget)
                return false;

            // Allow to save position on fisheye cameras only if dewarping is enabled.
            const bool isFisheyeCamera = widget->resource()->getDewarpingParams().enabled;
            if (isFisheyeCamera)
                return widget->item()->dewarpingParams().enabled;

            return true;
        });
}

ConditionWrapper isEntropixCamera()
{
    return new CustomBoolCondition(
        [](const Parameters& parameters, QnWorkbenchContext* /*context*/)
        {
            const auto& resouces = parameters.resources();

            return std::all_of(resouces.begin(), resouces.end(),
                [](const QnResourcePtr& resource)
                {
                    const auto& camera = resource.dynamicCast<QnVirtualCameraResource>();
                    if (!camera)
                        return false;

                    return camera->hasCombinedSensors();
                });
        });
}

} // namespace condition

} // namespace action
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
