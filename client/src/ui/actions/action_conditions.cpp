#include "action_conditions.h"

#include <QtWidgets/QAction>

#include <api/app_server_connection.h>

#include <common/common_module.h>

#include <utils/common/warnings.h>
#include <utils/network/router.h>

#include <core/resource_management/resource_criterion.h>
#include <core/resource_management/resource_pool.h>

#include <core/resource/layout_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/videowall_item_index.h>

#include <core/ptz/ptz_controller_pool.h>
#include <core/ptz/abstract_ptz_controller.h>
#include <recording/time_period_list.h>
#include <camera/resource_display.h>

#include <client/client_settings.h>

#include <plugins/storage/file_storage/layout_storage_resource.h>

#include <recording/time_period.h>

#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/watchers/workbench_schedule_watcher.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_auto_starter.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/dialogs/ptz_manage_dialog.h>

#include "action_parameter_types.h"
#include "action_manager.h"

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


bool QnPreviewSearchModeCondition::isPreviewSearchMode() const {
    return context()->workbench()->currentLayout()->data().contains(Qn::LayoutSearchStateRole);
}

Qn::ActionVisibility QnPreviewSearchModeCondition::check(const QnActionParameters &parameters) {
    Q_UNUSED(parameters)
        if (m_hide == isPreviewSearchMode())
            return Qn::InvisibleAction;
    return Qn::EnabledAction;
}

QnConjunctionActionCondition::QnConjunctionActionCondition(const QList<QnActionCondition *> conditions, QObject *parent) :
    QnActionCondition(parent),
    m_conditions(conditions)
{}

QnConjunctionActionCondition::QnConjunctionActionCondition(QnActionCondition *condition1, QnActionCondition *condition2, QObject *parent) :
    QnActionCondition(parent)
{
    m_conditions.append(condition1);
    m_conditions.append(condition2);
}

QnConjunctionActionCondition::QnConjunctionActionCondition(QnActionCondition *condition1, QnActionCondition *condition2, QnActionCondition *condition3, QObject *parent) :
    QnActionCondition(parent)
{
    m_conditions.append(condition1);
    m_conditions.append(condition2);
    m_conditions.append(condition3);
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
    foreach(QnResourceWidget *widget, widgets) {
        if(!widget)
            continue;

        if(!widget->resource()->hasFlags(Qn::motion))
            continue;

        if(!widget->zoomRect().isNull())
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

        if (!(widget->visibleButtons() & QnResourceWidget::InfoButton))
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

Qn::ActionVisibility QnCheckFileSignatureActionCondition::check(const QnResourceWidgetList &widgets) {
    foreach(QGraphicsItem *item, widgets) {
        QnResourceWidget *widget = item->isWidget() ? qobject_cast<QnResourceWidget *>(item->toGraphicsObject()) : NULL;
        if(widget == NULL)
            continue;

        bool isUnsupported = 
            (widget->resource()->flags() & (Qn::network | Qn::still_image | Qn::server)) ||
            !(widget->resource()->flags() & Qn::utc); // TODO: #GDM #Common this is wrong, we need a flag for exported files.
        if(isUnsupported)
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


Qn::ActionVisibility QnResourceRemovalActionCondition::check(const QnResourceList &resources) {
    foreach(const QnResourcePtr &resource, resources) {
        if(!resource)
            continue; /* OK to remove. */

        if(resource->hasFlags(Qn::layout) && !resource->hasFlags(Qn::local))
            continue; /* OK to remove. */

        if(resource->hasFlags(Qn::user) || resource->hasFlags(Qn::videowall))
            continue; /* OK to remove. */

        if(resource->hasFlags(Qn::live_cam))
            continue; /* OK to remove. */

        if(resource->hasFlags(Qn::remote_server)) // TODO: #Elric move this to permissions.
            if(resource->getStatus() == Qn::Offline)
                continue; /* Can remove only if offline. */

        return Qn::InvisibleAction;
    }

    return Qn::EnabledAction;
}


Qn::ActionVisibility QnRenameActionCondition::check(const QnActionParameters &parameters) {
    Qn::NodeType nodeType = parameters.argument<Qn::NodeType>(Qn::NodeTypeRole, Qn::ResourceNode);

    switch (nodeType) {
    case Qn::ResourceNode:
    case Qn::EdgeNode:
        return QnEdgeServerCondition::check(parameters.resources());
    case Qn::RecorderNode:
    case Qn::VideoWallItemNode:
    case Qn::VideoWallMatrixNode:
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

Qn::ActionVisibility QnSaveLayoutAsActionCondition::check(const QnResourceList &resources) {
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
    if(widget->resource()->flags() & (Qn::server | Qn::videowall))
        return Qn::InvisibleAction;

    QString url = widget->resource()->getUrl().toLower();
    if((widget->resource()->flags() & Qn::still_image) && !url.endsWith(lit(".jpg")) && !url.endsWith(lit(".jpeg")))
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
    if(!(m_periodTypes & period.type())) {
        return m_nonMatchingVisibility;
    } else {
        return Qn::EnabledAction;
    }
}

Qn::ActionVisibility QnExportActionCondition::check(const QnActionParameters &parameters) {
    if(!parameters.hasArgument(Qn::TimePeriodRole))
        return Qn::InvisibleAction;

    QnTimePeriod period = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);
    if(!(Qn::NormalTimePeriod & period.type()))
        return Qn::DisabledAction;

    if(m_centralItemRequired && !context()->workbench()->item(Qn::CentralRole))
        return Qn::DisabledAction;

    // Export selection
    if (m_centralItemRequired) {
        QnResourcePtr resource = parameters.resource();
        if(resource->flags() & Qn::sync) {
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
#ifdef QN_ENABLE_BOOKMARKS
    if(!parameters.hasArgument(Qn::TimePeriodRole))
        return Qn::InvisibleAction;

    QnTimePeriod period = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);
    if(!(Qn::NormalTimePeriod & period.type()))
        return Qn::DisabledAction;

    if(!context()->workbench()->item(Qn::CentralRole))
        return Qn::DisabledAction;

    QnResourcePtr resource = parameters.resource();
    if(resource->flags() & Qn::sync) {
        QnTimePeriodList periods = parameters.argument<QnTimePeriodList>(Qn::TimePeriodsRole);
        if(!periods.intersects(period))
            return Qn::DisabledAction;
    }

    return Qn::EnabledAction;
#else
    Q_UNUSED(parameters)
    return Qn::InvisibleAction;
#endif
}


Qn::ActionVisibility QnModifyBookmarkActionCondition::check(const QnActionParameters &parameters) {
#ifdef QN_ENABLE_BOOKMARKS
    if(!parameters.hasArgument(Qn::CameraBookmarkRole))
        return Qn::InvisibleAction;
    return Qn::EnabledAction;
#else
    Q_UNUSED(parameters)
    return Qn::InvisibleAction;
#endif
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

    if (context()->workbench()->currentLayout()->data().contains(Qn::LayoutSearchStateRole))
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

Qn::ActionVisibility QnToggleTourActionCondition::check(const QnActionParameters &parameters) {
    Q_UNUSED(parameters)
    if (isVideoWallReviewMode())
        return Qn::InvisibleAction;
    return context()->workbench()->currentLayout()->items().size() <= 1 ? Qn::DisabledAction : Qn::EnabledAction;
}

Qn::ActionVisibility QnArchiveActionCondition::check(const QnResourceList &resources) {
    if(resources.size() != 1)
        return Qn::InvisibleAction;

    bool watchable = !(resources[0]->flags() & Qn::live) || (accessController()->globalPermissions() & Qn::GlobalViewArchivePermission);
    return watchable ? Qn::EnabledAction : Qn::InvisibleAction;

    // TODO: #Elric this will fail (?) if we have sync with some UTC resource on the scene.
}

Qn::ActionVisibility QnToggleTitleBarActionCondition::check(const QnActionParameters &) {
    return action(Qn::EffectiveMaximizeAction)->isChecked() ? Qn::EnabledAction : Qn::InvisibleAction;
}

Qn::ActionVisibility QnNoArchiveActionCondition::check(const QnActionParameters &) {
    return (accessController()->globalPermissions() & Qn::GlobalViewArchivePermission) ? Qn::InvisibleAction : Qn::EnabledAction;
}

Qn::ActionVisibility QnOpenInFolderActionCondition::check(const QnResourceList &resources) {
    if(resources.size() != 1)
        return Qn::InvisibleAction;

    QnResourcePtr resource = resources[0];
    bool isLocalResource = resource->hasFlags(Qn::url | Qn::local | Qn::media)
            && !resource->getUrl().startsWith(QnLayoutFileStorageResource::layoutPrefix());
    bool isExportedLayout = resource->hasFlags(Qn::url | Qn::local | Qn::layout);

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

    if(!accessController()->hasPermissions(resource, Qn::EditLayoutSettingsPermission))
        return Qn::InvisibleAction;
    return Qn::EnabledAction;

//    bool isExportedLayout = resource->hasFlags(Qn::url | Qn::local | Qn::layout);
//    return resource->hasFlags(Qn::layout) && !isExportedLayout
//            ? Qn::EnabledAction
//            : Qn::InvisibleAction;
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

Qn::ActionVisibility QnTreeNodeTypeCondition::check(const QnActionParameters &parameters) {
    if (parameters.hasArgument(Qn::NodeTypeRole)) {
        Qn::NodeType nodeType = parameters.argument(Qn::NodeTypeRole).value<Qn::NodeType>();
        return (nodeType == m_nodeType) ? Qn::EnabledAction : Qn::InvisibleAction;
    }
    return Qn::EnabledAction;
}

Qn::ActionVisibility QnOpenInCurrentLayoutActionCondition::check(const QnResourceList &resources) {
    QnLayoutResourcePtr layout = context()->workbench()->currentLayout()->resource();
    bool isExportedLayout = snapshotManager()->isFile(layout);

    foreach (const QnResourcePtr &resource, resources) {
        if (resource->getStatus() == Qn::Incompatible)
            continue;

        //TODO: #GDM #Common refactor duplicated code
        bool isServer = resource->hasFlags(Qn::server);
        bool isMediaResource = resource->hasFlags(Qn::media);
        bool isLocalResource = resource->hasFlags(Qn::url | Qn::local | Qn::media)
            && !resource->getUrl().startsWith(QnLayoutFileStorageResource::layoutPrefix());
        bool allowed = isServer || isMediaResource;
        bool forbidden = isExportedLayout && (isServer || isLocalResource);
        if(allowed && !forbidden)
            return Qn::EnabledAction;
    }
    return Qn::InvisibleAction;
}

Qn::ActionVisibility QnOpenInNewEntityActionCondition::check(const QnResourceList &resources) {
    foreach(const QnResourcePtr &resource, resources) {
        if (resource->hasFlags(Qn::desktop_camera))
            continue;
        if (resource->hasFlags(Qn::media) || resource->hasFlags(Qn::server))
            return Qn::EnabledAction;
    }

    return Qn::InvisibleAction;
}

Qn::ActionVisibility QnOpenInNewEntityActionCondition::check(const QnLayoutItemIndexList &layoutItems) {
    foreach(const QnLayoutItemIndex &index, layoutItems) {
        QnLayoutItemData itemData = index.layout()->getItem(index.uuid());
        if(itemData.zoomRect.isNull())
            return QnActionCondition::check(layoutItems);
    }

    return Qn::InvisibleAction;
}

Qn::ActionVisibility QnSetAsBackgroundActionCondition::check(const QnResourceList &resources) {
    if(resources.size() != 1)
        return Qn::InvisibleAction;
    QnResourcePtr resource = resources[0];
    if (!resource->hasFlags(Qn::url | Qn::local | Qn::still_image))
        return Qn::InvisibleAction;

    QnLayoutResourcePtr layout = context()->workbench()->currentLayout()->resource();
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
    return qnCommon->remoteGUID().isNull()
        ? Qn::InvisibleAction
        : Qn::EnabledAction;
}

Qn::ActionVisibility QnChangeResolutionActionCondition::check(const QnActionParameters &) {
    if (isVideoWallReviewMode())
        return Qn::InvisibleAction;

    if  (!context()->user())
        return Qn::InvisibleAction;
    QnLayoutResourcePtr layout = context()->workbench()->currentLayout()->resource();
    if (snapshotManager()->isFile(layout))
        return Qn::InvisibleAction;
    else
        return Qn::EnabledAction;
}

Qn::ActionVisibility QnShowcaseActionCondition::check(const QnActionParameters &) {
    return qnSettings->isShowcaseEnabled() ? Qn::EnabledAction : Qn::InvisibleAction;
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

    if(m_current) {
        if (workbench()->currentLayout()->data().contains(Qn::VideoWallResourceRole))
            layouts << workbench()->currentLayout()->resource();
    } else {
        foreach(const QnResourcePtr &resource, resources) {
            if(!resource->hasFlags(Qn::videowall)) 
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
        return Qn::DisabledAction;
    }
    return QnActionCondition::check(parameters);
}

Qn::ActionVisibility QnResetVideoWallLayoutActionCondition::check(const QnActionParameters &parameters) {
    if (!context()->user() || parameters.videoWallItems().isEmpty())
        return Qn::InvisibleAction;

    QnLayoutResourcePtr layout = parameters.argument<QnLayoutResourcePtr>(Qn::LayoutResourceRole,
                                                                          workbench()->currentLayout()->resource());
    if (layout->data().contains(Qn::VideoWallResourceRole))
        return Qn::InvisibleAction;

    if (snapshotManager()->isFile(layout))
        return Qn::InvisibleAction;

    if (accessController()->globalPermissions() & Qn::GlobalEditVideoWallPermission)
        return Qn::EnabledAction;

    return Qn::InvisibleAction;
}

Qn::ActionVisibility QnDetachFromVideoWallActionCondition::check(const QnActionParameters &parameters) {
    if (!context()->user() || parameters.videoWallItems().isEmpty())
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
        if (m_isEdgeServer ^ QnMediaServerResource::isHiddenServer(resource))
            return Qn::InvisibleAction;
    return Qn::EnabledAction;
}

Qn::ActionVisibility QnResourceStatusActionCondition::check(const QnResourceList &resources) {
    bool found = false;
    foreach (const QnResourcePtr &resource, resources) {
        if (resource->getStatus() != m_status) {
            if (m_all)
                return Qn::InvisibleAction;
        } else {
            found = true;
        }
    }
    return found ? Qn::EnabledAction : Qn::InvisibleAction;
}

Qn::ActionVisibility QnDesktopCameraActionCondition::check(const QnActionParameters &parameters) {
#ifdef Q_OS_WIN
    if (!context()->user())
        return Qn::InvisibleAction;

    QString userName = context()->user()->getName();
    foreach (const QnResourcePtr &resource, qnResPool->getResourcesWithFlag(Qn::desktop_camera)) 
        if (resource->getUniqueId() == QnAppServerConnectionFactory::clientGuid())
            return Qn::EnabledAction;
    
    return Qn::InvisibleAction;
   
#else
    Q_UNUSED(parameters)
    return Qn::InvisibleAction;
#endif
}

Qn::ActionVisibility QnAutoStartAllowedActionCodition::check(const QnActionParameters &parameters) {
    Q_UNUSED(parameters)
    if(!context()->instance<QnWorkbenchAutoStarter>()->isSupported())
        return Qn::InvisibleAction;
    return Qn::EnabledAction;
}


QnDisjunctionActionCondition::QnDisjunctionActionCondition(const QList<QnActionCondition *> conditions, QObject *parent) :
    QnActionCondition(parent),
    m_conditions(conditions)
{
}

QnDisjunctionActionCondition::QnDisjunctionActionCondition(QnActionCondition *condition1, QnActionCondition *condition2, QObject *parent) :
    QnActionCondition(parent)
{
    m_conditions.append(condition1);
    m_conditions.append(condition2);
}

QnDisjunctionActionCondition::QnDisjunctionActionCondition(QnActionCondition *condition1, QnActionCondition *condition2, QnActionCondition *condition3, QObject *parent) :
    QnActionCondition(parent)
{
    m_conditions.append(condition1);
    m_conditions.append(condition2);
    m_conditions.append(condition3);
}

Qn::ActionVisibility QnDisjunctionActionCondition::check(const QnActionParameters &parameters) {
    Qn::ActionVisibility result = Qn::InvisibleAction;
    foreach (QnActionCondition *condition, m_conditions)
        result = qMax(result, condition->check(parameters));

    return result;
}


Qn::ActionVisibility QnServerWebPageCondition::check(const QnResourceList &resources) {
    if (resources.size() != 1)
        return Qn::InvisibleAction;

    QnMediaServerResourcePtr server = resources.first().dynamicCast<QnMediaServerResource>();
    if (!server)
        return Qn::InvisibleAction;

    QnRoute route = QnRouter::instance()->routeTo(server->getId());
    return route.length() == 1 ? Qn::EnabledAction : Qn::DisabledAction;
}
