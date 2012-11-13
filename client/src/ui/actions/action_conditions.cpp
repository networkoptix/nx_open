#include "action_conditions.h"

#include <utils/common/warnings.h>
#include <core/resource_managment/resource_criterion.h>
#include <core/resource_managment/resource_pool.h>
#include <recording/time_period_list.h>

#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/watchers/workbench_schedule_watcher.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>

#include "action_parameter_types.h"
#include "action_manager.h"

QnActionCondition::QnActionCondition(QObject *parent): 
    QObject(parent),
    QnWorkbenchContextAware(parent)
{}

Qn::ActionVisibility QnActionCondition::check(const QnResourceList &) { 
    return Qn::InvisibleAction; 
};

Qn::ActionVisibility QnActionCondition::check(const QnLayoutItemIndexList &layoutItems) { 
    return check(QnActionParameterTypes::resources(layoutItems));
};

Qn::ActionVisibility QnActionCondition::check(const QnResourceWidgetList &widgets) { 
    return check(QnActionParameterTypes::layoutItems(widgets));
};

Qn::ActionVisibility QnActionCondition::check(const QnWorkbenchLayoutList &layouts) { 
    return check(QnActionParameterTypes::resources(layouts));
};

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

Qn::ActionVisibility QnItemZoomedActionCondition::check(const QnResourceWidgetList &widgets) {
    if(widgets.size() != 1 || !widgets[0])
        return Qn::InvisibleAction;

    return ((widgets[0]->item() == workbench()->item(Qn::ZoomedRole)) == m_requiredZoomedState) ? Qn::EnabledAction : Qn::InvisibleAction;
}


Qn::ActionVisibility QnSmartSearchActionCondition::check(const QnResourceWidgetList &widgets) {
    foreach(QnResourceWidget *widget, widgets) {
        if(!widget)
            continue;

        bool isCamera = widget->resource()->flags() & QnResource::network;
        if(!isCamera)
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

        if (!widget->visibleButtons() & QnResourceWidget::InfoButton)
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

        bool isUnsupported = widget->resource()->flags() & (QnResource::network | QnResource::still_image | QnResource::server);
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

        if(resource->hasFlags(QnResource::layout) && !resource->hasFlags(QnResource::local))
            continue; /* OK to remove. */

        if(resource->hasFlags(QnResource::user))
            continue; /* OK to remove. */

        if(resource->hasFlags(QnResource::remote_server) || resource->hasFlags(QnResource::live_cam)) // TODO: move this to permissions.
            if(resource->getStatus() == QnResource::Offline)
                continue; /* Can remove only if offline. */

        return Qn::InvisibleAction;
    }

    return Qn::EnabledAction;
}


Qn::ActionVisibility QnLayoutItemRemovalActionCondition::check(const QnLayoutItemIndexList &layoutItems) {
    foreach(const QnLayoutItemIndex &item, layoutItems)
        if(!accessController()->hasPermissions(item.layout(), Qn::WritePermission | Qn::AddRemoveItemsPermission))
            return Qn::DisabledAction;

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

    if(snapshotManager()->isSaveable(layout)) {
        return Qn::EnabledAction;
    } else {
        return Qn::DisabledAction;
    }
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
    if(widget->resource()->flags() & (QnResource::still_image | QnResource::server))
        return Qn::InvisibleAction;

    Qn::RenderStatus renderStatus = widget->currentRenderStatus();
    if(renderStatus == Qn::NothingRendered || renderStatus == Qn::CannotRender)
        return Qn::DisabledAction;

    return Qn::EnabledAction;
}

Qn::ActionVisibility QnTimePeriodActionCondition::check(const QnActionParameters &parameters) {
    if(!parameters.hasArgument(Qn::TimePeriodParameter))
        return Qn::InvisibleAction;

    if(m_centralItemRequired && !context()->workbench()->item(Qn::CentralRole))
        return m_nonMatchingVisibility;

    QnTimePeriod period = parameters.argument<QnTimePeriod>(Qn::TimePeriodParameter);
    if(!(m_periodTypes & period.type())) {
        return m_nonMatchingVisibility;
    } else {
        return Qn::EnabledAction;
    }
}

Qn::ActionVisibility QnExportActionCondition::check(const QnActionParameters &parameters) {
    if(!parameters.hasArgument(Qn::TimePeriodParameter))
        return Qn::InvisibleAction;

    QnTimePeriod period = parameters.argument<QnTimePeriod>(Qn::TimePeriodParameter);
    if(!(Qn::NormalTimePeriod & period.type()))
        return Qn::DisabledAction;

    if(m_centralItemRequired && !context()->workbench()->item(Qn::CentralRole))
        return Qn::DisabledAction;

    if (m_centralItemRequired) {
        QnResourcePtr resource = parameters.resource();
        if(resource->flags() & QnResource::sync) {
            QnTimePeriodList periods = parameters.argument<QnTimePeriodList>(Qn::TimePeriodsParameter);
            if(!periods.intersects(period))
                return Qn::DisabledAction;
        }
    }    
    return Qn::EnabledAction;
}

Qn::ActionVisibility QnPanicActionCondition::check(const QnActionParameters &) {
    return context()->instance<QnWorkbenchScheduleWatcher>()->isScheduleEnabled() ? Qn::EnabledAction : Qn::DisabledAction;
}

Qn::ActionVisibility QnToggleTourActionCondition::check(const QnActionParameters &) {
    return context()->workbench()->currentLayout()->items().size() <= 1 ? Qn::DisabledAction : Qn::EnabledAction;
}

Qn::ActionVisibility QnArchiveActionCondition::check(const QnResourceList &resources) {
    if(resources.size() != 1)
        return Qn::InvisibleAction;

    bool watchable = !(resources[0]->flags() & QnResource::live) || (accessController()->globalPermissions() & Qn::GlobalViewArchivePermission);
    return watchable ? Qn::EnabledAction : Qn::InvisibleAction;

    // TODO: this will fail (?) if we have sync with some UTC resource on the scene.
}

Qn::ActionVisibility QnToggleTitleBarActionCondition::check(const QnActionParameters &) {
    return action(Qn::EffectiveMaximizeAction)->isChecked() ? Qn::EnabledAction : Qn::InvisibleAction;
}

Qn::ActionVisibility QnNoArchiveActionCondition::check(const QnActionParameters &) {
    return (accessController()->globalPermissions() & Qn::GlobalViewArchivePermission) ? Qn::InvisibleAction : Qn::EnabledAction;
}


Qn::ActionVisibility QnDisconnectActionCondition::check(const QnActionParameters &) {
    return (context()->user()) ? Qn::EnabledAction : Qn::InvisibleAction;
}

Qn::ActionVisibility QnOpenInFolderActionCondition::check(const QnResourceList &resources) {
    if(resources.size() != 1)
        return Qn::InvisibleAction;

    QnResourcePtr resource = resources[0];
    return resource->hasFlags(QnResource::url | QnResource::local | QnResource::media) && !resource->getUrl().startsWith(QLatin1String("layout:")) ? Qn::EnabledAction : Qn::InvisibleAction;
}
