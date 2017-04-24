#include "action_conditions.h"

#include <QtWidgets/QAction>

#include <boost/range/algorithm/count_if.hpp>

#include <api/app_server_connection.h>

#include <common/common_module.h>

#include <network/router.h>

#include <core/resource_access/resource_access_subject.h>
#include <core/resource_access/resource_access_filter.h>
#include <core/resource_access/providers/resource_access_provider.h>
#include <core/resource_management/layout_tour_manager.h>
#include <core/resource_management/resource_criterion.h>
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
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/dialogs/ptz_manage_dialog.h>

#include <nx/vms/utils/platform/autorun.h>

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
    ConjunctionCondition(const ConditionPtr& l, const ConditionPtr& r, QObject* parent):
        Condition(parent),
        l(l),
        r(r)
    {
    }

    virtual ActionVisibility check(const Parameters& parameters) override
    {
        // A bit of lazyness.
        auto first = l->check(parameters);
        if (first == InvisibleAction)
            return first;
        return qMin(first, r->check(parameters));
    }

private:
    ConditionPtr l;
    ConditionPtr r;
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
    DisjunctionCondition(const ConditionPtr& l, const ConditionPtr& r, QObject* parent):
        Condition(parent),
        l(l),
        r(r)
    {
    }

    virtual ActionVisibility check(const Parameters& parameters) override
    {
        // A bit of lazyness.
        auto first = l->check(parameters);
        if (first == EnabledAction)
            return first;
        return qMax(first, r->check(parameters));
    }

private:
    ConditionPtr l;
    ConditionPtr r;
};

class NegativeCondition: public Condition
{
public:
    NegativeCondition(const ConditionPtr& condition, QObject* parent):
        Condition(parent),
        m_condition(condition)
    {
    }

    virtual ActionVisibility check(const Parameters& parameters) override
    {
        ActionVisibility result = m_condition->check(parameters);
        if (result == InvisibleAction)
            return EnabledAction;
        return InvisibleAction;
    }

private:
    ConditionPtr m_condition;
};

class CustomBoolCondition: public Condition
{
public:
    using CheckDelegate = std::function<bool(
        QnWorkbenchContext* context, const Parameters& parameters)>;

    CustomBoolCondition(CheckDelegate delegate, QObject* parent):
        Condition(parent),
        m_delegate(delegate)
    {
    }

    virtual ActionVisibility check(const Parameters& parameters) override
    {
        return m_delegate(context(), parameters)
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
        QnWorkbenchContext* context, const Parameters& parameters)>;

    CustomCondition(CheckDelegate delegate, QObject* parent):
        Condition(parent),
        m_delegate(delegate)
    {
    }

    virtual ActionVisibility check(const Parameters& parameters) override
    {
        return m_delegate(context(), parameters);
    }

private:
    CheckDelegate m_delegate;
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

Condition::Condition(QObject* parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
}

ActionVisibility Condition::check(const QnResourceList &)
{
    return InvisibleAction;
}

ActionVisibility Condition::check(const QnLayoutItemIndexList& layoutItems)
{
    return check(ParameterTypes::resources(layoutItems));
}

ActionVisibility Condition::check(const QnResourceWidgetList& widgets)
{
    return check(ParameterTypes::layoutItems(widgets));
}

ActionVisibility Condition::check(const QnWorkbenchLayoutList& layouts)
{
    return check(ParameterTypes::resources(layouts));
}

ActionVisibility Condition::check(const Parameters& parameters)
{
    switch (parameters.type())
    {
        case ResourceType:
            return check(parameters.resources());
        case WidgetType:
            return check(parameters.widgets());
        case LayoutType:
            return check(parameters.layouts());
        case LayoutItemType:
            return check(parameters.layoutItems());
        default:
            NX_EXPECT(false, lm("Invalid parameter type '%1'.").arg(parameters.items().typeName()));
            return InvisibleAction;
    }
}

ConditionPtr operator&&(const ConditionPtr& l, const ConditionPtr& r)
{
    NX_EXPECT(l.data() && r.data() && l->parent());
    if (l.data() && r.data() && l->parent())
        return new ConjunctionCondition(l, r, l->parent());
    return ConditionPtr();
}

ConditionPtr operator||(const ConditionPtr& l, const ConditionPtr& r)
{
    NX_EXPECT(l.data() && r.data() && l->parent());
    if (l.data() && r.data() && l->parent())
        return new DisjunctionCondition(l, r, l->parent());
    return ConditionPtr();
}

ConditionPtr operator!(const ConditionPtr& l)
{
    NX_EXPECT(l.data() && l->parent());
    if (l.data() && l->parent())
        return new NegativeCondition(l, l->parent());
    return ConditionPtr();
}

VideoWallReviewModeCondition::VideoWallReviewModeCondition(QObject* parent):
    Condition(parent)
{
}

bool VideoWallReviewModeCondition::isVideoWallReviewMode() const
{
    return context()->workbench()->currentLayout()->data().contains(Qn::VideoWallResourceRole);
}

ActionVisibility VideoWallReviewModeCondition::check(const Parameters& /*parameters*/)
{
    if (isVideoWallReviewMode())
        return EnabledAction;
    return InvisibleAction;
}

LayoutTourReviewModeCondition::LayoutTourReviewModeCondition(QObject* parent):
    Condition(parent)
{
}

ActionVisibility LayoutTourReviewModeCondition::check(const Parameters& /*parameters*/)
{
    const bool isLayoutTourReviewMode = context()->workbench()->currentLayout()->data()
        .contains(Qn::LayoutTourUuidRole);

    return isLayoutTourReviewMode
        ? EnabledAction
        : InvisibleAction;
}

RequiresOwnerCondition::RequiresOwnerCondition(QObject* parent):
    Condition(parent)
{

}

ActionVisibility RequiresOwnerCondition::check(const Parameters& /*parameters*/)
{
    if (context()->user() && context()->user()->isOwner())
        return EnabledAction;
    return InvisibleAction;
}

ItemZoomedCondition::ItemZoomedCondition(bool requiredZoomedState, QObject* parent):
    Condition(parent),
    m_requiredZoomedState(requiredZoomedState)
{

}

ActionVisibility ItemZoomedCondition::check(const QnResourceWidgetList& widgets)
{
    if (widgets.size() != 1 || !widgets[0])
        return InvisibleAction;

    if (widgets[0]->resource()->flags() & Qn::videowall)
        return InvisibleAction;

    return ((widgets[0]->item() == workbench()->item(Qn::ZoomedRole)) == m_requiredZoomedState) ? EnabledAction : InvisibleAction;
}

SmartSearchCondition::SmartSearchCondition(bool requiredGridDisplayValue, QObject* parent):
    Condition(parent),
    m_hasRequiredGridDisplayValue(true),
    m_requiredGridDisplayValue(requiredGridDisplayValue)
{

}

SmartSearchCondition::SmartSearchCondition(QObject* parent):
    Condition(parent),
    m_hasRequiredGridDisplayValue(false),
    m_requiredGridDisplayValue(false)
{

}

ActionVisibility SmartSearchCondition::check(const QnResourceWidgetList& widgets)
{
    auto pureIoModule = [](const QnResourcePtr& resource)
        {
            if (!resource->hasFlags(Qn::io_module))
                return false; //quick check

            QnMediaResourcePtr mediaResource = resource.dynamicCast<QnMediaResource>();
            return mediaResource && !mediaResource->hasVideo(0);
        };

    foreach(QnResourceWidget *widget, widgets)
    {
        if (!widget)
            continue;

        if (!widget->resource()->hasFlags(Qn::motion))
            continue;

        if (!widget->zoomRect().isNull())
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

DisplayInfoCondition::DisplayInfoCondition(bool requiredDisplayInfoValue, QObject* parent):
    Condition(parent),
    m_hasRequiredDisplayInfoValue(true),
    m_requiredDisplayInfoValue(requiredDisplayInfoValue)
{

}

DisplayInfoCondition::DisplayInfoCondition(QObject* parent):
    Condition(parent),
    m_hasRequiredDisplayInfoValue(false),
    m_requiredDisplayInfoValue(false)
{

}

ActionVisibility DisplayInfoCondition::check(const QnResourceWidgetList& widgets)
{
    foreach(QnResourceWidget *widget, widgets)
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

ClearMotionSelectionCondition::ClearMotionSelectionCondition(QObject* parent): Condition(parent)
{

}

ActionVisibility ClearMotionSelectionCondition::check(const QnResourceWidgetList& widgets)
{
    bool hasDisplayedGrid = false;

    foreach(QnResourceWidget *widget, widgets)
    {
        if (!widget)
            continue;

        if (widget->options() & QnResourceWidget::DisplayMotion)
        {
            hasDisplayedGrid = true;

            if (QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget))
                foreach(const QRegion &region, mediaWidget->motionSelection())
                if (!region.isEmpty())
                    return EnabledAction;
        }
    }

    return hasDisplayedGrid ? DisabledAction : InvisibleAction;
}

CheckFileSignatureCondition::CheckFileSignatureCondition(QObject* parent): Condition(parent)
{

}

ActionVisibility CheckFileSignatureCondition::check(const QnResourceWidgetList& widgets)
{
    NX_ASSERT(widgets.size() == 1);
    for (auto widget : widgets)
    {
        if (!widget->resource()->hasFlags(Qn::exported_media))
            return InvisibleAction;
    }
    return EnabledAction;
}

ResourceCondition::ResourceCondition(const QnResourceCriterion &criterion,
    MatchMode matchMode,
    QObject* parent)
    :
    Condition(parent),
    m_criterion(criterion),
    m_matchMode(matchMode)
{
}

ResourceCondition::~ResourceCondition()
{
    return;
}

ActionVisibility ResourceCondition::check(const QnResourceList& resources)
{
    return checkInternal<QnResourcePtr>(resources) ? EnabledAction : InvisibleAction;
}

ActionVisibility ResourceCondition::check(const QnResourceWidgetList& widgets)
{
    return checkInternal<QnResourceWidget *>(widgets) ? EnabledAction : InvisibleAction;
}

template<class Item, class ItemSequence>
bool ResourceCondition::checkInternal(const ItemSequence &sequence)
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

bool ResourceCondition::checkOne(const QnResourcePtr &resource)
{
    return m_criterion.check(resource) == QnResourceCriterion::Accept;
}

bool ResourceCondition::checkOne(QnResourceWidget *widget)
{
    QnResourcePtr resource = ParameterTypes::resource(widget);
    return resource ? checkOne(resource) : false;
}

ActionVisibility ResourceRemovalCondition::check(const Parameters& parameters)
{
    Qn::NodeType nodeType = parameters.argument<Qn::NodeType>(Qn::NodeTypeRole, Qn::ResourceNode);
    if (nodeType == Qn::SharedLayoutNode || nodeType == Qn::SharedResourceNode)
        return InvisibleAction;

    QnUserResourcePtr owner = parameters.argument<QnUserResourcePtr>(Qn::UserResourceRole);
    bool ownResources = owner && owner == context()->user();

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

ActionVisibility StopSharingCondition::check(const Parameters& parameters)
{
    if (commonModule()->isReadOnly())
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
        : QnResourceAccessSubject(userRolesManager()->userRole(roleId));
    if (!subject.isValid())
        return InvisibleAction;

    for (auto resource : parameters.resources())
    {
        if (resourceAccessProvider()->accessibleVia(subject, resource) == QnAbstractResourceAccessProvider::Source::shared)
            return EnabledAction;
    }

    return DisabledAction;
}


RenameResourceCondition::RenameResourceCondition(QObject* parent): Condition(parent)
{

}

ActionVisibility RenameResourceCondition::check(const Parameters& parameters)
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

LayoutItemRemovalCondition::LayoutItemRemovalCondition(QObject* parent): Condition(parent)
{

}

ActionVisibility LayoutItemRemovalCondition::check(const QnLayoutItemIndexList& layoutItems)
{
    foreach(const QnLayoutItemIndex &item, layoutItems)
        if (!accessController()->hasPermissions(item.layout(), Qn::WritePermission | Qn::AddRemoveItemsPermission))
            return InvisibleAction;

    return EnabledAction;
}

SaveLayoutCondition::SaveLayoutCondition(bool isCurrent, QObject* parent): Condition(parent), m_current(isCurrent)
{

}

ActionVisibility SaveLayoutCondition::check(const QnResourceList& resources)
{
    QnLayoutResourcePtr layout;

    if (m_current)
    {
        layout = workbench()->currentLayout()->resource();
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

    if (snapshotManager()->isSaveable(layout))
    {
        return EnabledAction;
    }
    else
    {
        return DisabledAction;
    }
}

SaveLayoutAsCondition::SaveLayoutAsCondition(bool isCurrent, QObject* parent): Condition(parent), m_current(isCurrent)
{

}

ActionVisibility SaveLayoutAsCondition::check(const QnResourceList& resources)
{
    if (!context()->user())
        return InvisibleAction;

    QnLayoutResourcePtr layout;
    if (m_current)
    {
        layout = workbench()->currentLayout()->resource();
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

LayoutCountCondition::LayoutCountCondition(int minimalRequiredCount, QObject* parent): Condition(parent), m_minimalRequiredCount(minimalRequiredCount)
{

}

ActionVisibility LayoutCountCondition::check(const QnWorkbenchLayoutList &)
{
    if (workbench()->layouts().size() < m_minimalRequiredCount)
        return DisabledAction;

    return EnabledAction;
}


TakeScreenshotCondition::TakeScreenshotCondition(QObject* parent): Condition(parent)
{

}

ActionVisibility TakeScreenshotCondition::check(const QnResourceWidgetList& widgets)
{
    if (widgets.size() != 1)
        return InvisibleAction;

    QnResourceWidget *widget = widgets[0];
    if (widget->resource()->flags() & (Qn::still_image | Qn::server))
        return InvisibleAction;

    Qn::RenderStatus renderStatus = widget->renderStatus();
    if (renderStatus == Qn::NothingRendered || renderStatus == Qn::CannotRender)
        return DisabledAction;

    return EnabledAction;
}

AdjustVideoCondition::AdjustVideoCondition(QObject* parent): Condition(parent)
{

}

ActionVisibility AdjustVideoCondition::check(const QnResourceWidgetList& widgets)
{
    if (widgets.size() != 1)
        return InvisibleAction;

    QnResourceWidget *widget = widgets[0];
    if ((widget->resource()->flags() & (Qn::server | Qn::videowall))
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

TimePeriodCondition::TimePeriodCondition(TimePeriodTypes periodTypes, ActionVisibility nonMatchingVisibility, QObject* parent):
    Condition(parent),
    m_periodTypes(periodTypes),
    m_nonMatchingVisibility(nonMatchingVisibility)
{

}

ActionVisibility TimePeriodCondition::check(const Parameters& parameters)
{
    if (!parameters.hasArgument(Qn::TimePeriodRole))
        return InvisibleAction;

    QnTimePeriod period = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);
    if (m_periodTypes.testFlag(periodType(period)))
        return EnabledAction;

    return m_nonMatchingVisibility;
}

ExportCondition::ExportCondition(bool centralItemRequired, QObject* parent):
    Condition(parent),
    m_centralItemRequired(centralItemRequired)
{

}

ActionVisibility ExportCondition::check(const Parameters& parameters)
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
        if (containsAvailablePeriods && !context()->workbench()->item(Qn::CentralRole))
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

AddBookmarkCondition::AddBookmarkCondition(QObject* parent):
    Condition(parent)
{

}

ActionVisibility AddBookmarkCondition::check(const Parameters& parameters)
{
    if (!parameters.hasArgument(Qn::TimePeriodRole))
        return InvisibleAction;

    QnTimePeriod period = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);
    if (periodType(period) != NormalTimePeriod)
        return DisabledAction;

    if (!context()->workbench()->item(Qn::CentralRole))
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

ActionVisibility ModifyBookmarkCondition::check(const Parameters& parameters)
{
    if (!parameters.hasArgument(Qn::CameraBookmarkRole))
        return InvisibleAction;
    return EnabledAction;
}

ModifyBookmarkCondition::ModifyBookmarkCondition(QObject* parent):
    Condition(parent)
{

}

RemoveBookmarksCondition::RemoveBookmarksCondition(QObject* parent):
    Condition(parent)
{

}

ActionVisibility RemoveBookmarksCondition::check(const Parameters& parameters)
{
    if (!parameters.hasArgument(Qn::CameraBookmarkListRole))
        return InvisibleAction;

    if (parameters.argument(Qn::CameraBookmarkListRole).value<QnCameraBookmarkList>().isEmpty())
        return InvisibleAction;

    return EnabledAction;
}

PreviewCondition::PreviewCondition(QObject* parent): ExportCondition(true, parent)
{

}

ActionVisibility PreviewCondition::check(const Parameters& parameters)
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

    if (context()->workbench()->currentLayout()->isSearchLayout())
        return EnabledAction;

#if 0
    if (camera->isGroupPlayOnly())
        return InvisibleAction;
#endif
    return ExportCondition::check(parameters);
}

PanicCondition::PanicCondition(QObject* parent):
    Condition(parent)
{

}

ActionVisibility PanicCondition::check(const Parameters &)
{
    return context()->instance<QnWorkbenchScheduleWatcher>()->isScheduleEnabled() ? EnabledAction : DisabledAction;
}

ToggleTourCondition::ToggleTourCondition(QObject* parent): Condition(parent)
{

}

ActionVisibility ToggleTourCondition::check(const Parameters& parameters)
{
    const auto tourId = parameters.argument(Qn::UuidRole).value<QnUuid>();
    if (tourId.isNull())
    {
        if (context()->workbench()->currentLayout()->items().size() > 1)
            return EnabledAction;
    }
    else
    {
        const auto tour = qnLayoutTourManager->tour(tourId);
        if (tour.isValid() && tour.items.size() > 0)
            return EnabledAction;
    }

    return DisabledAction;
}

StartCurrentLayoutTourCondition::StartCurrentLayoutTourCondition(QObject* parent): Condition(parent)
{

}

ActionVisibility StartCurrentLayoutTourCondition::check(
    const Parameters& /*parameters*/)
{
    const auto tourId = context()->workbench()->currentLayout()->data()
        .value(Qn::LayoutTourUuidRole).value<QnUuid>();
    const auto tour = qnLayoutTourManager->tour(tourId);
    if (tour.isValid() && tour.items.size() > 0)
        return EnabledAction;
    return DisabledAction;
}


ArchiveCondition::ArchiveCondition(QObject* parent): Condition(parent)
{

}

ActionVisibility ArchiveCondition::check(const QnResourceList& resources)
{
    if (resources.size() != 1)
        return InvisibleAction;

    bool hasFootage = resources[0]->flags().testFlag(Qn::video);
    return hasFootage
        ? EnabledAction
        : InvisibleAction;
}

TimelineVisibleCondition::TimelineVisibleCondition(QObject* parent): Condition(parent)
{

}

ActionVisibility TimelineVisibleCondition::check(const Parameters& /*parameters*/)
{
    return context()->navigator()->isPlayingSupported()
        ? EnabledAction
        : InvisibleAction;
}

ToggleTitleBarCondition::ToggleTitleBarCondition(QObject* parent): Condition(parent)
{

}

ActionVisibility ToggleTitleBarCondition::check(const Parameters &)
{
    return action(QnActions::EffectiveMaximizeAction)->isChecked() ? EnabledAction : InvisibleAction;
}

NoArchiveCondition::NoArchiveCondition(QObject* parent): Condition(parent)
{

}

ActionVisibility NoArchiveCondition::check(const Parameters &)
{
    return accessController()->hasGlobalPermission(Qn::GlobalViewArchivePermission) ? InvisibleAction : EnabledAction;
}

OpenInFolderCondition::OpenInFolderCondition(QObject* parent): Condition(parent)
{

}

ActionVisibility OpenInFolderCondition::check(const QnResourceList& resources)
{
    if (resources.size() != 1)
        return InvisibleAction;

    QnResourcePtr resource = resources[0];
    bool isLocalResource = resource->hasFlags(Qn::local_media)
        && !resource->hasFlags(Qn::exported);
    bool isExportedLayout = resource->hasFlags(Qn::exported_layout);

    return isLocalResource || isExportedLayout ? EnabledAction : InvisibleAction;
}

ActionVisibility OpenInFolderCondition::check(const QnLayoutItemIndexList& layoutItems)
{
    foreach(const QnLayoutItemIndex &index, layoutItems)
    {
        QnLayoutItemData itemData = index.layout()->getItem(index.uuid());
        if (itemData.zoomRect.isNull())
            return Condition::check(layoutItems);
    }

    return InvisibleAction;
}

LayoutSettingsCondition::LayoutSettingsCondition(QObject* parent): Condition(parent)
{

}

ActionVisibility LayoutSettingsCondition::check(const QnResourceList& resources)
{
    if (resources.size() > 1)
        return InvisibleAction;

    QnResourcePtr resource;
    if (resources.size() > 0)
        resource = resources[0];
    else
        resource = context()->workbench()->currentLayout()->resource();

    if (!resource)
        return InvisibleAction;

    if (!accessController()->hasPermissions(resource, Qn::EditLayoutSettingsPermission))
        return InvisibleAction;
    return EnabledAction;
}

CreateZoomWindowCondition::CreateZoomWindowCondition(QObject* parent): Condition(parent)
{

}

ActionVisibility CreateZoomWindowCondition::check(const QnResourceWidgetList& widgets)
{
    if (widgets.size() != 1)
        return InvisibleAction;

    // TODO: #Elric there probably exists a better way to check it all.

    QnMediaResourceWidget *widget = dynamic_cast<QnMediaResourceWidget *>(widgets[0]);
    if (!widget)
        return InvisibleAction;

    if (display()->zoomTargetWidget(widget))
        return InvisibleAction;

    /*if(widget->display()->videoLayout() && widget->display()->videoLayout()->channelCount() > 1)
        return InvisibleAction;*/

    return EnabledAction;
}

TreeNodeTypeCondition::TreeNodeTypeCondition(Qn::NodeType nodeType, QObject* parent):
    Condition(parent),
    m_nodeTypes({nodeType})
{
}

TreeNodeTypeCondition::TreeNodeTypeCondition(QList<Qn::NodeType> nodeTypes, QObject* parent):
    Condition(parent),
    m_nodeTypes(nodeTypes.toSet())
{
}

ActionVisibility TreeNodeTypeCondition::check(const Parameters& parameters)
{
    if (parameters.hasArgument(Qn::NodeTypeRole))
    {
        Qn::NodeType nodeType = parameters.argument(Qn::NodeTypeRole).value<Qn::NodeType>();
        return m_nodeTypes.contains(nodeType)
            ? EnabledAction
            : InvisibleAction;
    }
    return EnabledAction;
}

NewUserLayoutCondition::NewUserLayoutCondition(QObject* parent): Condition(parent)
{

}

ActionVisibility NewUserLayoutCondition::check(const Parameters& parameters)
{
    if (!parameters.hasArgument(Qn::NodeTypeRole))
        return InvisibleAction;

    Qn::NodeType nodeType = parameters.argument(Qn::NodeTypeRole).value<Qn::NodeType>();

    /* Create layout for self. */
    if (nodeType == Qn::LayoutsNode)
        return EnabledAction;

    /* Create layout for other user. */
    if (nodeType != Qn::ResourceNode)
        return InvisibleAction;
    QnUserResourcePtr user = parameters.resource().dynamicCast<QnUserResource>();
    if (!user || user == context()->user())
        return InvisibleAction;

    return accessController()->canCreateLayout(user->getId())
        ? EnabledAction
        : InvisibleAction;
}


OpenInLayoutCondition::OpenInLayoutCondition(QObject* parent): Condition(parent)
{

}

ActionVisibility OpenInLayoutCondition::check(const Parameters& parameters)
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
    auto openableInLayout = [](const QnResourcePtr& resource)
        {
            return QnResourceAccessFilter::isShareableMedia(resource)
                || resource->hasFlags(Qn::local_media);
        };

    if (!layout)
        return any_of(resources, openableInLayout);

    bool isExportedLayout = layout->isFile();

    for (const auto& resource : resources)
    {
        if (!openableInLayout(resource))
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


OpenInCurrentLayoutCondition::OpenInCurrentLayoutCondition(QObject* parent): OpenInLayoutCondition(parent)
{

}

ActionVisibility OpenInCurrentLayoutCondition::check(const QnResourceList& resources)
{
    QnLayoutResourcePtr layout = context()->workbench()->currentLayout()->resource();

    if (!layout)
        return InvisibleAction;

    return canOpen(resources, layout)
        ? EnabledAction
        : InvisibleAction;
}

ActionVisibility OpenInCurrentLayoutCondition::check(const Parameters& parameters)
{
    /* Make sure we will get to specialized implementation */
    return Condition::check(parameters);
}

OpenInNewEntityCondition::OpenInNewEntityCondition(QObject* parent): OpenInLayoutCondition(parent)
{

}

ActionVisibility OpenInNewEntityCondition::check(const QnResourceList& resources)
{
    return canOpen(resources, QnLayoutResourcePtr())
        ? EnabledAction
        : InvisibleAction;

}

ActionVisibility OpenInNewEntityCondition::check(const QnLayoutItemIndexList& layoutItems)
{
    foreach(const QnLayoutItemIndex &index, layoutItems)
    {
        QnLayoutItemData itemData = index.layout()->getItem(index.uuid());
        if (itemData.zoomRect.isNull())
            return Condition::check(layoutItems);
    }

    return InvisibleAction;
}

ActionVisibility OpenInNewEntityCondition::check(const Parameters& parameters)
{
    /* Make sure we will get to specialized implementation */
    return Condition::check(parameters);
}

SetAsBackgroundCondition::SetAsBackgroundCondition(QObject* parent): Condition(parent)
{

}

ActionVisibility SetAsBackgroundCondition::check(const QnResourceList& resources)
{
    if (resources.size() != 1)
        return InvisibleAction;
    QnResourcePtr resource = resources[0];
    if (!resource->hasFlags(Qn::url | Qn::local | Qn::still_image))
        return InvisibleAction;

    QnLayoutResourcePtr layout = context()->workbench()->currentLayout()->resource();
    if (!layout)
        return InvisibleAction;

    if (layout->locked())
        return DisabledAction;
    return EnabledAction;
}

ActionVisibility SetAsBackgroundCondition::check(const QnLayoutItemIndexList& layoutItems)
{
    foreach(const QnLayoutItemIndex &index, layoutItems)
    {
        QnLayoutItemData itemData = index.layout()->getItem(index.uuid());
        if (itemData.zoomRect.isNull())
            return Condition::check(layoutItems);
    }

    return InvisibleAction;
}

LoggedInCondition::LoggedInCondition(QObject* parent): Condition(parent)
{

}

ActionVisibility LoggedInCondition::check(const Parameters& /*parameters*/)
{
    return commonModule()->remoteGUID().isNull()
        ? InvisibleAction
        : EnabledAction;
}

BrowseLocalFilesCondition::BrowseLocalFilesCondition(QObject* parent):
    Condition(parent)
{
}

ActionVisibility BrowseLocalFilesCondition::check(const Parameters& /*parameters*/)
{
    const bool connected = !commonModule()->remoteGUID().isNull();
    return (connected ? InvisibleAction : EnabledAction);
}

ChangeResolutionCondition::ChangeResolutionCondition(QObject* parent):
    VideoWallReviewModeCondition(parent)
{
}

ActionVisibility ChangeResolutionCondition::check(const Parameters &)
{
    if (isVideoWallReviewMode())
        return InvisibleAction;

    if (!context()->user())
        return InvisibleAction;

    QnLayoutResourcePtr layout = context()->workbench()->currentLayout()->resource();
    if (!layout)
        return InvisibleAction;

    if (layout->isFile())
        return InvisibleAction;

    return EnabledAction;
}

PtzCondition::PtzCondition(Qn::PtzCapabilities capabilities, bool disableIfPtzDialogVisible, QObject* parent):
    Condition(parent),
    m_capabilities(capabilities),
    m_disableIfPtzDialogVisible(disableIfPtzDialogVisible)
{

}

ActionVisibility PtzCondition::check(const Parameters& parameters)
{
    bool isPreviewSearchMode =
        parameters.scope() == SceneScope &&
        context()->workbench()->currentLayout()->isSearchLayout();
    if (isPreviewSearchMode)
        return InvisibleAction;
    return Condition::check(parameters);
}

ActionVisibility PtzCondition::check(const QnResourceList& resources)
{
    foreach(const QnResourcePtr &resource, resources)
        if (!check(qnPtzPool->controller(resource)))
            return InvisibleAction;

    if (m_disableIfPtzDialogVisible && QnPtzManageDialog::instance() && QnPtzManageDialog::instance()->isVisible())
        return DisabledAction;

    return EnabledAction;
}

ActionVisibility PtzCondition::check(const QnResourceWidgetList& widgets)
{
    foreach(QnResourceWidget *widget, widgets)
    {
        QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget);
        if (!mediaWidget)
            return InvisibleAction;

        if (!check(mediaWidget->ptzController()))
            return InvisibleAction;

        if (!mediaWidget->zoomRect().isNull())
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

NonEmptyVideowallCondition::NonEmptyVideowallCondition(QObject* parent): Condition(parent)
{

}

ActionVisibility NonEmptyVideowallCondition::check(const QnResourceList& resources)
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

SaveVideowallReviewCondition::SaveVideowallReviewCondition(bool isCurrent, QObject* parent): Condition(parent), m_current(isCurrent)
{

}

ActionVisibility SaveVideowallReviewCondition::check(const QnResourceList& resources)
{
    QnLayoutResourceList layouts;

    if (m_current)
    {
        auto layout = workbench()->currentLayout()->resource();
        if (!layout)
            return InvisibleAction;

        if (workbench()->currentLayout()->data().contains(Qn::VideoWallResourceRole))
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
        if (snapshotManager()->isModified(layout))
            return EnabledAction;

    return DisabledAction;
}

RunningVideowallCondition::RunningVideowallCondition(QObject* parent): Condition(parent)
{

}

ActionVisibility RunningVideowallCondition::check(const QnResourceList& resources)
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


StartVideowallCondition::StartVideowallCondition(QObject* parent): Condition(parent)
{

}

ActionVisibility StartVideowallCondition::check(const QnResourceList& resources)
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


IdentifyVideoWallCondition::IdentifyVideoWallCondition(QObject* parent): RunningVideowallCondition(parent)
{

}

ActionVisibility IdentifyVideoWallCondition::check(const Parameters& parameters)
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
    ActionVisibility baseResult = Condition::check(parameters);
    if (baseResult != EnabledAction)
        return InvisibleAction;
    return EnabledAction;
}

DetachFromVideoWallCondition::DetachFromVideoWallCondition(QObject* parent): Condition(parent)
{

}

ActionVisibility DetachFromVideoWallCondition::check(const Parameters& parameters)
{
    if (!context()->user() || parameters.videoWallItems().isEmpty())
        return InvisibleAction;

    if (commonModule()->isReadOnly())
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

StartVideoWallControlCondition::StartVideoWallControlCondition(QObject* parent): Condition(parent)
{

}

ActionVisibility StartVideoWallControlCondition::check(const Parameters& parameters)
{
    if (!context()->user() || parameters.videoWallItems().isEmpty())
        return InvisibleAction;

    foreach(const QnVideoWallItemIndex &index, parameters.videoWallItems())
    {
        if (!index.isValid())
            continue;

        return EnabledAction;
    }
    return InvisibleAction;
}


RotateItemCondition::RotateItemCondition(QObject* parent): Condition(parent)
{

}

ActionVisibility RotateItemCondition::check(const QnResourceWidgetList& widgets)
{
    foreach(QnResourceWidget *widget, widgets)
    {
        if (widget->options() & QnResourceWidget::WindowRotationForbidden)
            return InvisibleAction;
    }
    return EnabledAction;
}

LightModeCondition::LightModeCondition(Qn::LightModeFlags flags, QObject* parent):
    Condition(parent),
    m_lightModeFlags(flags)
{

}

ActionVisibility LightModeCondition::check(const Parameters& /*parameters*/)
{

    if (qnSettings->lightMode() & m_lightModeFlags)
        return InvisibleAction;

    return EnabledAction;
}

EdgeServerCondition::EdgeServerCondition(bool isEdgeServer, QObject* parent):
    Condition(parent),
    m_isEdgeServer(isEdgeServer)
{

}

ActionVisibility EdgeServerCondition::check(const QnResourceList& resources)
{
    foreach(const QnResourcePtr &resource, resources)
        if (m_isEdgeServer ^ QnMediaServerResource::isEdgeServer(resource))
            return InvisibleAction;
    return EnabledAction;
}

ResourceStatusCondition::ResourceStatusCondition(const QSet<Qn::ResourceStatus> statuses, bool allResources, QObject* parent):
    Condition(parent), m_statuses(statuses), m_all(allResources)
{

}

ResourceStatusCondition::ResourceStatusCondition(Qn::ResourceStatus status, bool allResources, QObject* parent):
    Condition(parent), m_statuses(QSet<Qn::ResourceStatus>() << status), m_all(allResources)
{

}

ActionVisibility ResourceStatusCondition::check(const QnResourceList& resources)
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

DesktopCameraCondition::DesktopCameraCondition(QObject* parent): Condition(parent)
{

}

ActionVisibility DesktopCameraCondition::check(const Parameters& /*parameters*/)
{
    const auto screenRecordingAction = action(QnActions::ToggleScreenRecordingAction);
    if (screenRecordingAction)
    {

        if (!context()->user())
            return InvisibleAction;

        /* Do not check real pointer type to speed up check. */
        QnResourcePtr desktopCamera = resourcePool()->getResourceByUniqueId(commonModule()->moduleGUID().toString());
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

AutoStartAllowedCondition::AutoStartAllowedCondition(QObject* parent): Condition(parent)
{

}

ActionVisibility AutoStartAllowedCondition::check(const Parameters& /*parameters*/)
{
    if (!nx::vms::utils::isAutoRunSupported())
        return InvisibleAction;
    return EnabledAction;
}


ItemsCountCondition::ItemsCountCondition(int count, QObject* parent):
    Condition(parent),
    m_count(count)
{

}

ActionVisibility ItemsCountCondition::check(const Parameters& /*parameters*/)
{
    int count = workbench()->currentLayout()->items().size();

    return (m_count == MultipleItems && count > 1) || (m_count == count) ? EnabledAction : InvisibleAction;
}

IoModuleCondition::IoModuleCondition(QObject* parent): Condition(parent)
{

}

ActionVisibility IoModuleCondition::check(const QnResourceList& resources)
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

MergeToCurrentSystemCondition::MergeToCurrentSystemCondition(QObject* parent): Condition(parent)
{

}

ActionVisibility MergeToCurrentSystemCondition::check(const QnResourceList& resources)
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


FakeServerCondition::FakeServerCondition(bool allResources, QObject* parent):
    Condition(parent),
    m_all(allResources)
{

}

ActionVisibility FakeServerCondition::check(const QnResourceList& resources)
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

CloudServerCondition::CloudServerCondition(MatchMode matchMode, QObject* parent):
    Condition(parent),
    m_matchMode(matchMode)
{
}

ActionVisibility CloudServerCondition::check(const QnResourceList& resources)
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

ConditionPtr isPreviewSearchMode(QObject* parent)
{
    return new CustomBoolCondition(
        [](QnWorkbenchContext* context, const Parameters& parameters)
        {
            return parameters.scope() == SceneScope
                && context->workbench()->currentLayout()->isSearchLayout();
        },
        parent);
}

ConditionPtr isSafeMode(QObject* parent)
{
    return new CustomBoolCondition(
        [](QnWorkbenchContext* context, const Parameters& /*parameters*/)
        {
            return context->commonModule()->isReadOnly();
        },
        parent);
}

ConditionPtr hasFlags(Qn::ResourceFlags flags, MatchMode matchMode, QObject* parent)
{
    return new ResourceCondition(QnResourceCriterionExpressions::hasFlags(flags), matchMode, parent);
}

} // namespace condition


} // namespace action
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
