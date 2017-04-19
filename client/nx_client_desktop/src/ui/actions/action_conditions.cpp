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

#include "action_parameter_types.h"
#include "action_manager.h"
#include <core/resource/fake_media_server.h>

using boost::algorithm::any_of;
using boost::algorithm::all_of;

namespace {

Qn::TimePeriodType periodType(const QnTimePeriod& period)
{
    if (period.isNull())
        return Qn::NullTimePeriod;

    if (period.isEmpty())
        return Qn::EmptyTimePeriod;

    return Qn::NormalTimePeriod;
}

} // namespace

QnActionCondition::QnActionCondition(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{}

Qn::ActionVisibility QnActionCondition::check(const QnResourceList &) {
    return Qn::InvisibleAction;
}

Qn::ActionVisibility QnActionCondition::check(const QnLayoutItemIndexList &layoutItems) {
    return check(QnActionParameterTypes::resources(layoutItems));
}

Qn::ActionVisibility QnActionCondition::check(const QnResourceWidgetList &widgets) {
    return check(QnActionParameterTypes::layoutItems(widgets));
}

Qn::ActionVisibility QnActionCondition::check(const QnWorkbenchLayoutList &layouts) {
    return check(QnActionParameterTypes::resources(layouts));
}

Qn::ActionVisibility QnActionCondition::check(const QnActionParameters &parameters) {
    switch(parameters.type()) {
    case Qn::ResourceType:
        return check(parameters.resources());
    case Qn::WidgetType:
        return check(parameters.widgets());
    case Qn::LayoutType:
        return check(parameters.layouts());
    case Qn::LayoutItemType:
        return check(parameters.layoutItems());
    default:
        qnWarning("Invalid action condition parameter type '%1'.", parameters.items().typeName());
        return Qn::InvisibleAction;
    }
}

 bool QnVideoWallReviewModeCondition::isVideoWallReviewMode() const {
    return context()->workbench()->currentLayout()->data().contains(Qn::VideoWallResourceRole);
}

Qn::ActionVisibility QnVideoWallReviewModeCondition::check(const QnActionParameters &parameters) {
    Q_UNUSED(parameters)
    if (m_hide == isVideoWallReviewMode())
        return Qn::InvisibleAction;
    return Qn::EnabledAction;
}

Qn::ActionVisibility QnLayoutTourReviewModeCondition::check(const QnActionParameters& /*parameters*/)
{
    const bool isLayoutTourReviewMode = context()->workbench()->currentLayout()->data()
        .contains(Qn::LayoutTourUuidRole);

    return isLayoutTourReviewMode
        ? Qn::EnabledAction
        : Qn::InvisibleAction;
}

bool QnPreviewSearchModeCondition::isPreviewSearchMode(const QnActionParameters &parameters) const {
    return
        parameters.scope() == Qn::SceneScope &&
        context()->workbench()->currentLayout()->isSearchLayout();
}

Qn::ActionVisibility QnPreviewSearchModeCondition::check(const QnActionParameters &parameters) {
    Q_UNUSED(parameters)
    if (m_hide == isPreviewSearchMode(parameters))
        return Qn::InvisibleAction;
    return Qn::EnabledAction;
}

Qn::ActionVisibility QnForbiddenInSafeModeCondition::check(const QnActionParameters &parameters) {
    Q_UNUSED(parameters)
    if (commonModule()->isReadOnly())
        return Qn::InvisibleAction;
    return Qn::EnabledAction;
}

Qn::ActionVisibility QnRequiresOwnerCondition::check(const QnActionParameters &parameters)
{
    Q_UNUSED(parameters);
    if (context()->user() && context()->user()->isOwner())
        return Qn::EnabledAction;
    return Qn::InvisibleAction;
}

QnConjunctionActionCondition::QnConjunctionActionCondition(const QList<QnActionCondition *> conditions, QObject *parent) :
    QnActionCondition(parent)
{
    for (auto condition: conditions)
    {
        if (condition)
            m_conditions.push_back(condition);
    }
}

QnConjunctionActionCondition::QnConjunctionActionCondition(
    QnActionCondition *condition1,
    QnActionCondition *condition2,
    QObject *parent)
    :
    QnConjunctionActionCondition({condition1, condition2}, parent)
{
}

QnConjunctionActionCondition::QnConjunctionActionCondition(
    QnActionCondition *condition1,
    QnActionCondition *condition2,
    QnActionCondition *condition3,
    QObject *parent)
    :
    QnConjunctionActionCondition({condition1, condition2, condition3}, parent)
{
}

QnConjunctionActionCondition::QnConjunctionActionCondition(
    QnActionCondition *condition1,
    QnActionCondition *condition2,
    QnActionCondition *condition3,
    QnActionCondition *condition4,
    QObject *parent)
    :
    QnConjunctionActionCondition({condition1, condition2, condition3, condition4}, parent)
{
}


Qn::ActionVisibility QnConjunctionActionCondition::check(const QnActionParameters &parameters) {
    Qn::ActionVisibility result = Qn::EnabledAction;
    foreach (QnActionCondition *condition, m_conditions)
        result = qMin(result, condition->check(parameters));

    return result;
}

Qn::ActionVisibility QnNegativeActionCondition::check(const QnActionParameters &parameters) {
    Qn::ActionVisibility result = m_condition->check(parameters);
    if (result == Qn::InvisibleAction)
        return Qn::EnabledAction;
    else
        return Qn::InvisibleAction;
}

Qn::ActionVisibility QnItemZoomedActionCondition::check(const QnResourceWidgetList &widgets) {
    if(widgets.size() != 1 || !widgets[0])
        return Qn::InvisibleAction;

    if (widgets[0]->resource()->flags() & Qn::videowall)
        return Qn::InvisibleAction;

    return ((widgets[0]->item() == workbench()->item(Qn::ZoomedRole)) == m_requiredZoomedState) ? Qn::EnabledAction : Qn::InvisibleAction;
}

Qn::ActionVisibility QnSmartSearchActionCondition::check(const QnResourceWidgetList &widgets) {
    auto pureIoModule = [](const QnResourcePtr &resource) {
        if (!resource->hasFlags(Qn::io_module))
            return false; //quick check

        QnMediaResourcePtr mediaResource = resource.dynamicCast<QnMediaResource>();
        return mediaResource && !mediaResource->hasVideo(0);
    };

    foreach(QnResourceWidget *widget, widgets) {
        if(!widget)
            continue;

        if(!widget->resource()->hasFlags(Qn::motion))
            continue;

        if(!widget->zoomRect().isNull())
            continue;

        if (pureIoModule(widget->resource()))
            continue;

        if(m_hasRequiredGridDisplayValue) {
            if(static_cast<bool>(widget->options() & QnResourceWidget::DisplayMotion) == m_requiredGridDisplayValue)
                return Qn::EnabledAction;
        } else {
            return Qn::EnabledAction;
        }
    }

    return Qn::InvisibleAction;
}

Qn::ActionVisibility QnDisplayInfoActionCondition::check(const QnResourceWidgetList &widgets) {
    foreach(QnResourceWidget *widget, widgets) {
        if(!widget)
            continue;

        if (!(widget->visibleButtons() & Qn::InfoButton))
            continue;

        if(m_hasRequiredDisplayInfoValue) {
            if(static_cast<bool>(widget->options() & QnResourceWidget::DisplayInfo) == m_requiredDisplayInfoValue)
                return Qn::EnabledAction;
        } else {
            return Qn::EnabledAction;
        }
    }

    return Qn::InvisibleAction;
}

Qn::ActionVisibility QnClearMotionSelectionActionCondition::check(const QnResourceWidgetList &widgets) {
    bool hasDisplayedGrid = false;

    foreach(QnResourceWidget *widget, widgets) {
        if(!widget)
            continue;

        if(widget->options() & QnResourceWidget::DisplayMotion) {
            hasDisplayedGrid = true;

            if(QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget))
                foreach(const QRegion &region, mediaWidget->motionSelection())
                    if(!region.isEmpty())
                        return Qn::EnabledAction;
        }
    }

    return hasDisplayedGrid ? Qn::DisabledAction : Qn::InvisibleAction;
}

Qn::ActionVisibility QnCheckFileSignatureActionCondition::check(const QnResourceWidgetList &widgets)
{
    NX_ASSERT(widgets.size() == 1);
    for (auto widget: widgets)
    {
        if (!widget->resource()->hasFlags(Qn::exported_media))
            return Qn::InvisibleAction;
    }
    return Qn::EnabledAction;
}

QnResourceActionCondition::QnResourceActionCondition(const QnResourceCriterion &criterion, Qn::MatchMode matchMode, QObject *parent):
    QnActionCondition(parent),
    m_criterion(criterion),
    m_matchMode(matchMode)
{}

QnResourceActionCondition::~QnResourceActionCondition() {
    return;
}

Qn::ActionVisibility QnResourceActionCondition::check(const QnResourceList &resources) {
    return checkInternal<QnResourcePtr>(resources) ? Qn::EnabledAction : Qn::InvisibleAction;
}

Qn::ActionVisibility QnResourceActionCondition::check(const QnResourceWidgetList &widgets) {
    return checkInternal<QnResourceWidget *>(widgets) ? Qn::EnabledAction : Qn::InvisibleAction;
}

template<class Item, class ItemSequence>
bool QnResourceActionCondition::checkInternal(const ItemSequence &sequence) {
    int count = 0;

    foreach(const Item &item, sequence) {
        bool matches = checkOne(item);

        if(matches && m_matchMode == Qn::Any)
            return true;

        if(!matches && m_matchMode == Qn::All)
            return false;

        if(matches)
            count++;
    }

    if(m_matchMode == Qn::Any) {
        return false;
    } else if(m_matchMode == Qn::All) {
        return true;
    } else if(m_matchMode == Qn::ExactlyOne) {
        return count == 1;
    } else {
        qnWarning("Invalid match mode '%1'.", static_cast<int>(m_matchMode));
        return false;
    }
}

bool QnResourceActionCondition::checkOne(const QnResourcePtr &resource) {
    return m_criterion.check(resource) == QnResourceCriterion::Accept;
}

bool QnResourceActionCondition::checkOne(QnResourceWidget *widget) {
    QnResourcePtr resource = QnActionParameterTypes::resource(widget);
    return resource ? checkOne(resource) : false;
}

Qn::ActionVisibility QnResourceRemovalActionCondition::check(const QnActionParameters &parameters)
{
    Qn::NodeType nodeType = parameters.argument<Qn::NodeType>(Qn::NodeTypeRole, Qn::ResourceNode);
    if (nodeType == Qn::SharedLayoutNode || nodeType == Qn::SharedResourceNode)
        return Qn::InvisibleAction;

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
                if (resource->resourcePool()->isAutoGeneratedLayout(layout))
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
        ? Qn::EnabledAction
        : Qn::InvisibleAction;
}

Qn::ActionVisibility QnStopSharingActionCondition::check(const QnActionParameters &parameters)
{
    if (commonModule()->isReadOnly())
        return Qn::InvisibleAction;

    Qn::NodeType nodeType = parameters.argument<Qn::NodeType>(Qn::NodeTypeRole, Qn::ResourceNode);
    if (nodeType != Qn::SharedLayoutNode)
        return Qn::InvisibleAction;

    auto user = parameters.argument<QnUserResourcePtr>(Qn::UserResourceRole);
    auto roleId = parameters.argument<QnUuid>(Qn::UuidRole);
    NX_ASSERT(user || !roleId.isNull());
    if (!user && roleId.isNull())
        return Qn::InvisibleAction;

    QnResourceAccessSubject subject = user
        ? QnResourceAccessSubject(user)
        : QnResourceAccessSubject(userRolesManager()->userRole(roleId));
    if (!subject.isValid())
        return Qn::InvisibleAction;

    for (auto resource: parameters.resources())
    {
        if (resourceAccessProvider()->accessibleVia(subject, resource) == QnAbstractResourceAccessProvider::Source::shared)
            return Qn::EnabledAction;
    }

    return Qn::DisabledAction;
}


Qn::ActionVisibility QnRenameResourceActionCondition::check(const QnActionParameters &parameters)
{
    Qn::NodeType nodeType = parameters.argument<Qn::NodeType>(Qn::NodeTypeRole, Qn::ResourceNode);

    switch (nodeType)
    {
        case Qn::ResourceNode:
        case Qn::SharedLayoutNode:
        case Qn::SharedResourceNode:
        {
            if (parameters.resources().size() != 1)
                return Qn::InvisibleAction;

            QnResourcePtr target = parameters.resource();
            if (!target)
                return Qn::InvisibleAction;

            /* Renaming users directly from resource tree is disabled due do digest re-generation need. */
            if (target->hasFlags(Qn::user))
                return Qn::InvisibleAction;

            /* Edge servers renaming is forbidden. */
            if (QnMediaServerResource::isEdgeServer(target))
                return Qn::InvisibleAction;

            /* Incompatible resources cannot be renamed */
            if (target.dynamicCast<QnFakeMediaServerResource>())
                return Qn::InvisibleAction;

            return Qn::EnabledAction;
        }
        case Qn::EdgeNode:
        case Qn::RecorderNode:
            return Qn::EnabledAction;
        default:
            break;
    }

    return Qn::InvisibleAction;
}

Qn::ActionVisibility QnLayoutItemRemovalActionCondition::check(const QnLayoutItemIndexList &layoutItems) {
    foreach(const QnLayoutItemIndex &item, layoutItems)
        if(!accessController()->hasPermissions(item.layout(), Qn::WritePermission | Qn::AddRemoveItemsPermission))
            return Qn::InvisibleAction;

    return Qn::EnabledAction;
}

Qn::ActionVisibility QnSaveLayoutActionCondition::check(const QnResourceList &resources) {
    QnLayoutResourcePtr layout;

    if(m_current) {
        layout = workbench()->currentLayout()->resource();
    } else {
        if(resources.size() != 1)
            return Qn::InvisibleAction;

        layout = resources[0].dynamicCast<QnLayoutResource>();
    }

    if(!layout)
        return Qn::InvisibleAction;

    if (layout->data().contains(Qn::VideoWallResourceRole))
        return Qn::InvisibleAction;

    if(snapshotManager()->isSaveable(layout)) {
        return Qn::EnabledAction;
    } else {
        return Qn::DisabledAction;
    }
}

Qn::ActionVisibility QnSaveLayoutAsActionCondition::check(const QnResourceList& resources)
{
    if (!context()->user())
        return Qn::InvisibleAction;

    QnLayoutResourcePtr layout;
    if (m_current)
    {
        layout = workbench()->currentLayout()->resource();
    }
    else
    {
        if (resources.size() != 1)
            return Qn::InvisibleAction;

        layout = resources[0].dynamicCast<QnLayoutResource>();
    }

    if (!layout)
        return Qn::InvisibleAction;

    if (resourcePool()->isAutoGeneratedLayout(layout))
        return Qn::InvisibleAction;

    if (layout->data().contains(Qn::VideoWallResourceRole))
        return Qn::InvisibleAction;

    /* Save as.. for exported layouts works very strange, disabling it for now. */
    if (layout->isFile())
        return Qn::InvisibleAction;

    return Qn::EnabledAction;
}

Qn::ActionVisibility QnLayoutCountActionCondition::check(const QnWorkbenchLayoutList &) {
    if(workbench()->layouts().size() < m_minimalRequiredCount)
        return Qn::DisabledAction;

    return Qn::EnabledAction;
}


Qn::ActionVisibility QnTakeScreenshotActionCondition::check(const QnResourceWidgetList &widgets) {
    if(widgets.size() != 1)
        return Qn::InvisibleAction;

    QnResourceWidget *widget = widgets[0];
    if(widget->resource()->flags() & (Qn::still_image | Qn::server))
        return Qn::InvisibleAction;

    Qn::RenderStatus renderStatus = widget->renderStatus();
    if(renderStatus == Qn::NothingRendered || renderStatus == Qn::CannotRender)
        return Qn::DisabledAction;

    return Qn::EnabledAction;
}

Qn::ActionVisibility QnAdjustVideoActionCondition::check(const QnResourceWidgetList &widgets) {
    if(widgets.size() != 1)
        return Qn::InvisibleAction;

    QnResourceWidget *widget = widgets[0];
    if((widget->resource()->flags() & (Qn::server | Qn::videowall))
        || (widget->resource()->flags().testFlag(Qn::web_page)))
        return Qn::InvisibleAction;

    QString url = widget->resource()->getUrl().toLower();
    if((widget->resource()->flags() & Qn::still_image) && !url.endsWith(lit(".jpg")) && !url.endsWith(lit(".jpeg")))
        return Qn::InvisibleAction;

    QnMediaResourcePtr mediaResource = widget->resource().dynamicCast<QnMediaResource>();
        if (mediaResource && !mediaResource->hasVideo(0))
            return Qn::InvisibleAction;

    Qn::RenderStatus renderStatus = widget->renderStatus();
    if(renderStatus == Qn::NothingRendered || renderStatus == Qn::CannotRender)
        return Qn::DisabledAction;

    return Qn::EnabledAction;
}

Qn::ActionVisibility QnTimePeriodActionCondition::check(const QnActionParameters &parameters) {
    if(!parameters.hasArgument(Qn::TimePeriodRole))
        return Qn::InvisibleAction;

    QnTimePeriod period = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);
    if (m_periodTypes.testFlag(periodType(period)))
        return Qn::EnabledAction;

    return m_nonMatchingVisibility;
}

Qn::ActionVisibility QnExportActionCondition::check(const QnActionParameters &parameters)
{
    if(!parameters.hasArgument(Qn::TimePeriodRole))
        return Qn::InvisibleAction;

    QnTimePeriod period = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);
    if (periodType(period) != Qn::NormalTimePeriod)
        return Qn::DisabledAction;

    // Export selection
    if (m_centralItemRequired) {

        const auto containsAvailablePeriods = parameters.hasArgument(Qn::TimePeriodsRole);

        /// If parameters contain periods it means we need current selected item
        if (containsAvailablePeriods && !context()->workbench()->item(Qn::CentralRole))
            return Qn::DisabledAction;

        QnResourcePtr resource = parameters.resource();
        if(containsAvailablePeriods && resource && resource->flags().testFlag(Qn::sync)) {
            QnTimePeriodList periods = parameters.argument<QnTimePeriodList>(Qn::TimePeriodsRole);
            if(!periods.intersects(period))
                return Qn::DisabledAction;
        }
    }
    // Export layout
    else {
        QnTimePeriodList periods = parameters.argument<QnTimePeriodList>(Qn::MergedTimePeriodsRole);
        if(!periods.intersects(period))
            return Qn::DisabledAction;
    }
    return Qn::EnabledAction;
}

Qn::ActionVisibility QnAddBookmarkActionCondition::check(const QnActionParameters &parameters) {
    if(!parameters.hasArgument(Qn::TimePeriodRole))
        return Qn::InvisibleAction;

    QnTimePeriod period = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);
    if (periodType(period) != Qn::NormalTimePeriod)
        return Qn::DisabledAction;

    if(!context()->workbench()->item(Qn::CentralRole))
        return Qn::DisabledAction;

    QnResourcePtr resource = parameters.resource();
    if (!resource->flags().testFlag(Qn::live))
        return Qn::InvisibleAction;

    if(resource->flags().testFlag(Qn::sync)) {
        QnTimePeriodList periods = parameters.argument<QnTimePeriodList>(Qn::TimePeriodsRole);
        if(!periods.intersects(period))
            return Qn::DisabledAction;
    }

    return Qn::EnabledAction;
}

Qn::ActionVisibility QnModifyBookmarkActionCondition::check(const QnActionParameters &parameters) {
    if(!parameters.hasArgument(Qn::CameraBookmarkRole))
        return Qn::InvisibleAction;
    return Qn::EnabledAction;
}

Qn::ActionVisibility QnRemoveBookmarksActionCondition::check(const QnActionParameters &parameters)
{
    if (!parameters.hasArgument(Qn::CameraBookmarkListRole))
        return Qn::InvisibleAction;

    if (parameters.argument(Qn::CameraBookmarkListRole).value<QnCameraBookmarkList>().isEmpty())
        return Qn::InvisibleAction;

    return Qn::EnabledAction;
}

Qn::ActionVisibility QnPreviewActionCondition::check(const QnActionParameters &parameters) {
    QnMediaResourcePtr media = parameters.resource().dynamicCast<QnMediaResource>();
    if(!media)
        return Qn::InvisibleAction;

    bool isImage = media->toResource()->flags() & Qn::still_image;
    if (isImage)
        return Qn::InvisibleAction;

    bool isPanoramic = media->getVideoLayout(0)->channelCount() > 1;
    if (isPanoramic)
        return Qn::InvisibleAction;

    if (context()->workbench()->currentLayout()->isSearchLayout())
        return Qn::EnabledAction;

#if 0
    if(camera->isGroupPlayOnly())
        return Qn::InvisibleAction;
#endif
    return QnExportActionCondition::check(parameters);
}

Qn::ActionVisibility QnPanicActionCondition::check(const QnActionParameters &) {
    return context()->instance<QnWorkbenchScheduleWatcher>()->isScheduleEnabled() ? Qn::EnabledAction : Qn::DisabledAction;
}

Qn::ActionVisibility QnToggleTourActionCondition::check(const QnActionParameters &parameters)
{
    const auto tourId = parameters.argument(Qn::UuidRole).value<QnUuid>();
    if (tourId.isNull())
    {
        if (context()->workbench()->currentLayout()->items().size() > 1)
            return Qn::EnabledAction;
    }
    else
    {
        const auto tour = qnLayoutTourManager->tour(tourId);
        if (tour.isValid() && tour.items.size() > 0)
            return Qn::EnabledAction;
    }

    return Qn::DisabledAction;
}

Qn::ActionVisibility QnStartCurrentLayoutTourActionCondition::check(
    const QnActionParameters& /*parameters*/)
{
    const auto tourId = context()->workbench()->currentLayout()->data()
        .value(Qn::LayoutTourUuidRole).value<QnUuid>();
    const auto tour = qnLayoutTourManager->tour(tourId);
    if (tour.isValid() && tour.items.size() > 0)
        return Qn::EnabledAction;
    return Qn::DisabledAction;
}


Qn::ActionVisibility QnArchiveActionCondition::check(const QnResourceList &resources)
{
    if (resources.size() != 1)
        return Qn::InvisibleAction;

    bool hasFootage = resources[0]->flags().testFlag(Qn::video);
    return hasFootage
        ? Qn::EnabledAction
        : Qn::InvisibleAction;
}

Qn::ActionVisibility QnTimelineVisibleActionCondition::check(const QnActionParameters &parameters)
{
    Q_UNUSED(parameters);
    return context()->navigator()->isPlayingSupported()
        ? Qn::EnabledAction
        : Qn::InvisibleAction;
}

Qn::ActionVisibility QnToggleTitleBarActionCondition::check(const QnActionParameters &) {
    return action(QnActions::EffectiveMaximizeAction)->isChecked() ? Qn::EnabledAction : Qn::InvisibleAction;
}

Qn::ActionVisibility QnNoArchiveActionCondition::check(const QnActionParameters &) {
    return accessController()->hasGlobalPermission(Qn::GlobalViewArchivePermission) ? Qn::InvisibleAction : Qn::EnabledAction;
}

Qn::ActionVisibility QnOpenInFolderActionCondition::check(const QnResourceList &resources)
{
    if(resources.size() != 1)
        return Qn::InvisibleAction;

    QnResourcePtr resource = resources[0];
    bool isLocalResource = resource->hasFlags(Qn::local_media)
        && !resource->hasFlags(Qn::exported);
    bool isExportedLayout = resource->hasFlags(Qn::exported_layout);

    return isLocalResource || isExportedLayout ? Qn::EnabledAction : Qn::InvisibleAction;
}

Qn::ActionVisibility QnOpenInFolderActionCondition::check(const QnLayoutItemIndexList &layoutItems) {
    foreach(const QnLayoutItemIndex &index, layoutItems) {
        QnLayoutItemData itemData = index.layout()->getItem(index.uuid());
        if(itemData.zoomRect.isNull())
            return QnActionCondition::check(layoutItems);
    }

    return Qn::InvisibleAction;
}

Qn::ActionVisibility QnLayoutSettingsActionCondition::check(const QnResourceList &resources) {
    if(resources.size() > 1)
        return Qn::InvisibleAction;

    QnResourcePtr resource;
    if (resources.size() > 0)
        resource = resources[0];
    else
        resource = context()->workbench()->currentLayout()->resource();

    if (!resource)
        return Qn::InvisibleAction;

    if(!accessController()->hasPermissions(resource, Qn::EditLayoutSettingsPermission))
        return Qn::InvisibleAction;
    return Qn::EnabledAction;
}

Qn::ActionVisibility QnCreateZoomWindowActionCondition::check(const QnResourceWidgetList &widgets) {
    if(widgets.size() != 1)
        return Qn::InvisibleAction;

    // TODO: #Elric there probably exists a better way to check it all.

    QnMediaResourceWidget *widget = dynamic_cast<QnMediaResourceWidget *>(widgets[0]);
    if(!widget)
        return Qn::InvisibleAction;

    if(display()->zoomTargetWidget(widget))
        return Qn::InvisibleAction;

    /*if(widget->display()->videoLayout() && widget->display()->videoLayout()->channelCount() > 1)
        return Qn::InvisibleAction;*/

    return Qn::EnabledAction;
}

QnTreeNodeTypeCondition::QnTreeNodeTypeCondition(Qn::NodeType nodeType, QObject *parent):
    QnActionCondition(parent),
    m_nodeTypes({nodeType})
{
}

QnTreeNodeTypeCondition::QnTreeNodeTypeCondition(QList<Qn::NodeType> nodeTypes, QObject *parent):
    QnActionCondition(parent),
    m_nodeTypes(nodeTypes.toSet())
{
}

Qn::ActionVisibility QnTreeNodeTypeCondition::check(const QnActionParameters &parameters)
{
    if (parameters.hasArgument(Qn::NodeTypeRole))
    {
        Qn::NodeType nodeType = parameters.argument(Qn::NodeTypeRole).value<Qn::NodeType>();
        return m_nodeTypes.contains(nodeType)
            ? Qn::EnabledAction
            : Qn::InvisibleAction;
    }
    return Qn::EnabledAction;
}

Qn::ActionVisibility QnNewUserLayoutActionCondition::check(const QnActionParameters &parameters)
{
    if (!parameters.hasArgument(Qn::NodeTypeRole))
        return Qn::InvisibleAction;

    Qn::NodeType nodeType = parameters.argument(Qn::NodeTypeRole).value<Qn::NodeType>();

    /* Create layout for self. */
    if (nodeType == Qn::LayoutsNode)
        return Qn::EnabledAction;

    /* Create layout for other user. */
    if (nodeType != Qn::ResourceNode)
        return Qn::InvisibleAction;
    QnUserResourcePtr user = parameters.resource().dynamicCast<QnUserResource>();
    if (!user || user == context()->user())
        return Qn::InvisibleAction;

    return accessController()->canCreateLayout(user->getId())
        ? Qn::EnabledAction
        : Qn::InvisibleAction;
}


Qn::ActionVisibility QnOpenInLayoutActionCondition::check(const QnActionParameters &parameters)
{
    auto layout = parameters.argument<QnLayoutResourcePtr>(Qn::LayoutResourceRole);
    if (!layout)
        return Qn::InvisibleAction;

    return canOpen(parameters.resources(), layout)
        ? Qn::EnabledAction
        : Qn::InvisibleAction;
}

bool QnOpenInLayoutActionCondition::canOpen(const QnResourceList &resources,
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


Qn::ActionVisibility QnOpenInCurrentLayoutActionCondition::check(const QnResourceList &resources) {
    QnLayoutResourcePtr layout = context()->workbench()->currentLayout()->resource();

    if (!layout)
        return Qn::InvisibleAction;

    return canOpen(resources, layout)
        ? Qn::EnabledAction
        : Qn::InvisibleAction;
}

Qn::ActionVisibility QnOpenInCurrentLayoutActionCondition::check(const QnActionParameters &parameters)
{
    /* Make sure we will get to specialized implementation */
    return QnActionCondition::check(parameters);
}

Qn::ActionVisibility QnOpenInNewEntityActionCondition::check(const QnResourceList& resources)
{
    return canOpen(resources, QnLayoutResourcePtr())
        ? Qn::EnabledAction
        : Qn::InvisibleAction;

}

Qn::ActionVisibility QnOpenInNewEntityActionCondition::check(const QnLayoutItemIndexList &layoutItems) {
    foreach(const QnLayoutItemIndex &index, layoutItems) {
        QnLayoutItemData itemData = index.layout()->getItem(index.uuid());
        if(itemData.zoomRect.isNull())
            return QnActionCondition::check(layoutItems);
    }

    return Qn::InvisibleAction;
}

Qn::ActionVisibility QnOpenInNewEntityActionCondition::check(const QnActionParameters &parameters)
{
    /* Make sure we will get to specialized implementation */
    return QnActionCondition::check(parameters);
}

Qn::ActionVisibility QnSetAsBackgroundActionCondition::check(const QnResourceList &resources)
{
    if(resources.size() != 1)
        return Qn::InvisibleAction;
    QnResourcePtr resource = resources[0];
    if (!resource->hasFlags(Qn::url | Qn::local | Qn::still_image))
        return Qn::InvisibleAction;

    QnLayoutResourcePtr layout = context()->workbench()->currentLayout()->resource();
    if (!layout)
        return Qn::InvisibleAction;

    if (layout->locked())
        return Qn::DisabledAction;
    return Qn::EnabledAction;
}

Qn::ActionVisibility QnSetAsBackgroundActionCondition::check(const QnLayoutItemIndexList &layoutItems) {
    foreach(const QnLayoutItemIndex &index, layoutItems) {
        QnLayoutItemData itemData = index.layout()->getItem(index.uuid());
        if(itemData.zoomRect.isNull())
            return QnActionCondition::check(layoutItems);
    }

    return Qn::InvisibleAction;
}

Qn::ActionVisibility QnLoggedInCondition::check(const QnActionParameters &parameters) {
    Q_UNUSED(parameters)
    return commonModule()->remoteGUID().isNull()
        ? Qn::InvisibleAction
        : Qn::EnabledAction;
}

QnBrowseLocalFilesCondition::QnBrowseLocalFilesCondition(QObject* parent) :
    QnActionCondition(parent)
{
}

Qn::ActionVisibility QnBrowseLocalFilesCondition::check(const QnActionParameters& parameters)
{
    Q_UNUSED(parameters);
    const bool connected = !commonModule()->remoteGUID().isNull();
    return (connected ? Qn::InvisibleAction : Qn::EnabledAction);
}

Qn::ActionVisibility QnChangeResolutionActionCondition::check(const QnActionParameters &) {
    if (isVideoWallReviewMode())
        return Qn::InvisibleAction;

    if  (!context()->user())
        return Qn::InvisibleAction;

    QnLayoutResourcePtr layout = context()->workbench()->currentLayout()->resource();
    if (!layout)
        return Qn::InvisibleAction;

    if (layout->isFile())
        return Qn::InvisibleAction;

    return Qn::EnabledAction;
}

Qn::ActionVisibility QnPtzActionCondition::check(const QnActionParameters &parameters) {
    bool isPreviewSearchMode =
        parameters.scope() == Qn::SceneScope &&
        context()->workbench()->currentLayout()->isSearchLayout();
    if (isPreviewSearchMode)
        return Qn::InvisibleAction;
    return QnActionCondition::check(parameters);
}

Qn::ActionVisibility QnPtzActionCondition::check(const QnResourceList &resources) {
    foreach(const QnResourcePtr &resource, resources)
        if(!check(qnPtzPool->controller(resource)))
            return Qn::InvisibleAction;

    if (m_disableIfPtzDialogVisible && QnPtzManageDialog::instance() && QnPtzManageDialog::instance()->isVisible())
        return Qn::DisabledAction;

    return Qn::EnabledAction;
}

Qn::ActionVisibility QnPtzActionCondition::check(const QnResourceWidgetList &widgets) {
    foreach(QnResourceWidget *widget, widgets) {
        QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget);
        if(!mediaWidget)
            return Qn::InvisibleAction;

        if(!check(mediaWidget->ptzController()))
            return Qn::InvisibleAction;

        if (!mediaWidget->zoomRect().isNull())
            return Qn::InvisibleAction;
    }

    if (m_disableIfPtzDialogVisible && QnPtzManageDialog::instance() && QnPtzManageDialog::instance()->isVisible())
        return Qn::DisabledAction;

    return Qn::EnabledAction;
}

bool QnPtzActionCondition::check(const QnPtzControllerPtr &controller) {
    return controller && controller->hasCapabilities(m_capabilities);
}

Qn::ActionVisibility QnNonEmptyVideowallActionCondition::check(const QnResourceList &resources) {
    foreach(const QnResourcePtr &resource, resources) {
        if(!resource->hasFlags(Qn::videowall))
            continue;

        QnVideoWallResourcePtr videowall = resource.dynamicCast<QnVideoWallResource>();
        if (!videowall)
            continue;

        if (videowall->items()->getItems().isEmpty())
            continue;

        return Qn::EnabledAction;
    }
    return Qn::InvisibleAction;
}

Qn::ActionVisibility QnSaveVideowallReviewActionCondition::check(const QnResourceList &resources) {
    QnLayoutResourceList layouts;

    if (m_current)
    {
        auto layout = workbench()->currentLayout()->resource();
        if (!layout)
            return Qn::InvisibleAction;

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
       return Qn::InvisibleAction;

    foreach (const QnLayoutResourcePtr &layout, layouts)
        if(snapshotManager()->isModified(layout))
            return Qn::EnabledAction;

    return Qn::DisabledAction;
}

Qn::ActionVisibility QnRunningVideowallActionCondition::check(const QnResourceList &resources) {
    bool hasNonEmptyVideowall = false;
    foreach(const QnResourcePtr &resource, resources) {
        if(!resource->hasFlags(Qn::videowall))
            continue;

        QnVideoWallResourcePtr videowall = resource.dynamicCast<QnVideoWallResource>();
        if (!videowall)
            continue;

        if (videowall->items()->getItems().isEmpty())
            continue;

        hasNonEmptyVideowall = true;
        if (videowall->onlineItems().isEmpty())
            continue;

        return Qn::EnabledAction;
    }

    return hasNonEmptyVideowall
        ? Qn::DisabledAction
        : Qn::InvisibleAction;
}


Qn::ActionVisibility QnStartVideowallActionCondition::check(const QnResourceList &resources) {
    QnUuid pcUuid = qnSettings->pcUuid();
    if (pcUuid.isNull())
        return Qn::InvisibleAction;

    bool hasAttachedItems = false;
    foreach(const QnResourcePtr &resource, resources) {
        if(!resource->hasFlags(Qn::videowall))
            continue;

        QnVideoWallResourcePtr videowall = resource.dynamicCast<QnVideoWallResource>();
        if (!videowall)
            continue;

        if (!videowall->pcs()->hasItem(pcUuid))
            continue;

        foreach (const QnVideoWallItem &item, videowall->items()->getItems()) {
            if (item.pcUuid != pcUuid)
                continue;

            if (!item.runtimeStatus.online)
               return Qn::EnabledAction;

            hasAttachedItems = true;
        }
    }

    return hasAttachedItems
        ? Qn::DisabledAction
        : Qn::InvisibleAction;
}


Qn::ActionVisibility QnIdentifyVideoWallActionCondition::check(const QnActionParameters &parameters) {
    if (parameters.videoWallItems().size() > 0) {
        // allow action if there is at least one online item
        foreach (const QnVideoWallItemIndex &index, parameters.videoWallItems()) {
            if (!index.isValid())
                continue;
            if (index.item().runtimeStatus.online)
                return Qn::EnabledAction;
        }
        return Qn::InvisibleAction;
    }

    /* 'Identify' action should not be displayed as disabled anyway. */
    Qn::ActionVisibility baseResult = QnActionCondition::check(parameters);
    if (baseResult != Qn::EnabledAction)
        return Qn::InvisibleAction;
    return Qn::EnabledAction;
}

Qn::ActionVisibility QnDetachFromVideoWallActionCondition::check(const QnActionParameters &parameters) {
    if (!context()->user() || parameters.videoWallItems().isEmpty())
        return Qn::InvisibleAction;

    if (commonModule()->isReadOnly())
        return Qn::InvisibleAction;

    foreach (const QnVideoWallItemIndex &index, parameters.videoWallItems()) {
        if (index.isNull())
            continue;
        if (!index.videowall()->items()->hasItem(index.uuid()))
            continue;
        if (index.videowall()->items()->getItem(index.uuid()).layout.isNull())
            continue;
        return Qn::EnabledAction;
    }
    return Qn::InvisibleAction;
}

Qn::ActionVisibility QnStartVideoWallControlActionCondition::check(const QnActionParameters &parameters) {
    if (!context()->user() || parameters.videoWallItems().isEmpty())
        return Qn::InvisibleAction;

    foreach (const QnVideoWallItemIndex &index, parameters.videoWallItems()) {
        if (!index.isValid())
            continue;

        return Qn::EnabledAction;
    }
    return Qn::InvisibleAction;
}


Qn::ActionVisibility QnRotateItemCondition::check(const QnResourceWidgetList &widgets) {
    foreach (QnResourceWidget *widget, widgets) {
        if (widget->options() & QnResourceWidget::WindowRotationForbidden)
            return Qn::InvisibleAction;
    }
    return Qn::EnabledAction;
}

Qn::ActionVisibility QnLightModeCondition::check(const QnActionParameters &parameters) {
    Q_UNUSED(parameters)

    if (qnSettings->lightMode() & m_lightModeFlags)
        return Qn::InvisibleAction;

    return Qn::EnabledAction;
}

Qn::ActionVisibility QnEdgeServerCondition::check(const QnResourceList &resources) {
    foreach (const QnResourcePtr &resource, resources)
        if (m_isEdgeServer ^ QnMediaServerResource::isEdgeServer(resource))
            return Qn::InvisibleAction;
    return Qn::EnabledAction;
}

Qn::ActionVisibility QnResourceStatusActionCondition::check(const QnResourceList &resources) {
    bool found = false;
    foreach (const QnResourcePtr &resource, resources) {
        if (!m_statuses.contains(resource->getStatus())) {
            if (m_all)
                return Qn::InvisibleAction;
        } else {
            found = true;
        }
    }
    return found ? Qn::EnabledAction : Qn::InvisibleAction;
}

Qn::ActionVisibility QnDesktopCameraActionCondition::check(const QnActionParameters &parameters) {
    Q_UNUSED(parameters);
#ifdef Q_OS_WIN
    if (!context()->user())
        return Qn::InvisibleAction;

    /* Do not check real pointer type to speed up check. */
    QnResourcePtr desktopCamera = resourcePool()->getResourceByUniqueId(commonModule()->moduleGUID().toString());
#ifdef DESKTOP_CAMERA_DEBUG
    NX_ASSERT(!desktopCamera || (desktopCamera->hasFlags(Qn::desktop_camera) && desktopCamera->getParentId() == commonModule()->remoteGUID()),
        Q_FUNC_INFO,
        "Desktop camera must have correct flags and parent (if exists)");
#endif
    if (desktopCamera && desktopCamera->hasFlags(Qn::desktop_camera))
        return Qn::EnabledAction;

    return Qn::DisabledAction;
#else
    return Qn::InvisibleAction;
#endif
}

Qn::ActionVisibility QnAutoStartAllowedActionCodition::check(const QnActionParameters &parameters) {
    Q_UNUSED(parameters)
    if (!nx::vms::utils::isAutoRunSupported())
        return Qn::InvisibleAction;
    return Qn::EnabledAction;
}


QnDisjunctionActionCondition::QnDisjunctionActionCondition(const QList<QnActionCondition *> conditions, QObject *parent) :
    QnActionCondition(parent)
{
    for (auto condition: conditions)
    {
        if (condition)
            m_conditions.push_back(condition);
    }
}

QnDisjunctionActionCondition::QnDisjunctionActionCondition(
    QnActionCondition* condition1,
    QnActionCondition* condition2,
    QObject* parent)
    :
    QnDisjunctionActionCondition({condition1, condition2}, parent)
{
}

QnDisjunctionActionCondition::QnDisjunctionActionCondition(
    QnActionCondition* condition1,
    QnActionCondition* condition2,
    QnActionCondition* condition3,
    QObject* parent)
    :
    QnDisjunctionActionCondition({condition1, condition2, condition3}, parent)
{
}

Qn::ActionVisibility QnDisjunctionActionCondition::check(const QnActionParameters &parameters) {
    Qn::ActionVisibility result = Qn::InvisibleAction;
    foreach (QnActionCondition *condition, m_conditions)
        result = qMax(result, condition->check(parameters));

    return result;
}

Qn::ActionVisibility QnItemsCountActionCondition::check(const QnActionParameters &parameters) {
    Q_UNUSED(parameters)

    int count = workbench()->currentLayout()->items().size();

    return (m_count == MultipleItems && count > 1) || (m_count == count) ? Qn::EnabledAction : Qn::InvisibleAction;
}

Qn::ActionVisibility QnIoModuleActionCondition::check(const QnResourceList &resources) {
    bool pureIoModules = boost::algorithm::all_of(resources, [](const QnResourcePtr &resource) {
        if (!resource->hasFlags(Qn::io_module))
            return false; //quick check

        QnMediaResourcePtr mediaResource = resource.dynamicCast<QnMediaResource>();
        return mediaResource && !mediaResource->hasVideo(0);
    });

    return pureIoModules ? Qn::EnabledAction : Qn::InvisibleAction;
}

Qn::ActionVisibility QnMergeToCurrentSystemActionCondition::check(const QnResourceList &resources) {
    if (resources.size() != 1)
        return Qn::InvisibleAction;

    QnMediaServerResourcePtr server = resources.first().dynamicCast<QnMediaServerResource>();
    if (!server)
        return Qn::InvisibleAction;

    Qn::ResourceStatus status = server->getStatus();
    if (status != Qn::Incompatible && status != Qn::Unauthorized)
        return Qn::InvisibleAction;

    if (server->getModuleInformation().ecDbReadOnly)
        return Qn::InvisibleAction;

    return Qn::EnabledAction;
}


Qn::ActionVisibility QnFakeServerActionCondition::check(const QnResourceList &resources)
{
    bool found = false;
    foreach (const QnResourcePtr &resource, resources)
    {
        QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;

        if (server.dynamicCast<QnFakeMediaServerResource>())
            found = true;
        else if (m_all)
            return Qn::InvisibleAction;
    }
    return found ? Qn::EnabledAction : Qn::InvisibleAction;
}

QnCloudServerActionCondition::QnCloudServerActionCondition(Qn::MatchMode matchMode, QObject* parent):
    QnActionCondition(parent),
    m_matchMode(matchMode)
{
}

Qn::ActionVisibility QnCloudServerActionCondition::check(const QnResourceList& resources)
{
    auto isCloudServer = [](const QnResourcePtr& resource)
        {
            return nx::network::isCloudServer(resource.dynamicCast<QnMediaServerResource>());
        };

    bool success = false;
    switch (m_matchMode)
    {
        case Qn::Any:
            success = any_of(resources, isCloudServer);
            break;
        case Qn::All:
            success = all_of(resources, isCloudServer);
            break;
        case Qn::ExactlyOne:
            success = (boost::count_if(resources, isCloudServer) == 1);
            break;
        default:
            break;
    }

    return success ? Qn::EnabledAction : Qn::InvisibleAction;
}
