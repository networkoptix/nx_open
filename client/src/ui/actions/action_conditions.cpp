#include "action_conditions.h"

#include <utils/common/warnings.h>
#include <core/resourcemanagment/resource_criterion.h>

#include <ui/graphics/items/resource_widget.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>

#include "action_target_types.h"
#include "action_manager.h"

QnActionCondition::QnActionCondition(QObject *parent): 
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    QnActionTargetTypes::initialize();
}

Qn::ActionVisibility QnActionCondition::check(const QnResourceList &) { 
    return Qn::InvisibleAction; 
};

Qn::ActionVisibility QnActionCondition::check(const QnLayoutItemIndexList &layoutItems) { 
    return check(QnActionTargetTypes::resources(layoutItems));
};

Qn::ActionVisibility QnActionCondition::check(const QnResourceWidgetList &widgets) { 
    return check(QnActionTargetTypes::layoutItems(widgets));
};

Qn::ActionVisibility QnActionCondition::check(const QnWorkbenchLayoutList &layouts) { 
    return check(QnActionTargetTypes::resources(layouts));
};

Qn::ActionVisibility QnActionCondition::check(const QVariant &items) {
    switch(QnActionTargetTypes::type(items)) {
    case Qn::ResourceType:
        return check(QnActionTargetTypes::resources(items));
    case Qn::WidgetType:
        return check(QnActionTargetTypes::widgets(items));
    case Qn::LayoutType:
        return check(QnActionTargetTypes::layouts(items));
    case Qn::LayoutItemType:
        return check(QnActionTargetTypes::layoutItems(items));
    default:
        qnWarning("Invalid action condition parameter type '%1'.", items.typeName());
        return Qn::InvisibleAction;
    }
}

Qn::ActionVisibility QnTargetlessActionCondition::check(const QVariant &items) {
    if(!items.isValid()) {
        return check(QnResourceList());
    } else {
        return base_type::check(items);
    }
}


Qn::ActionVisibility QnItemZoomedActionCondition::check(const QnResourceWidgetList &widgets) {
    if(widgets.size() != 1 || !widgets[0])
        return Qn::InvisibleAction;

    return ((widgets[0]->item() == workbench()->item(QnWorkbench::ZOOMED)) == m_requiredZoomedState) ? Qn::EnabledAction : Qn::InvisibleAction;
}


Qn::ActionVisibility QnMotionGridDisplayActionCondition::check(const QnResourceWidgetList &widgets) {
    foreach(QGraphicsItem *item, widgets) {
        QnResourceWidget *widget = item->isWidget() ? qobject_cast<QnResourceWidget *>(item->toGraphicsObject()) : NULL;
        if(widget == NULL)
            continue;

        bool isCamera = widget->resource()->flags() & QnResource::network;
        if(!isCamera)
            continue;

        if(m_hasRequiredGridDisplayValue) {
            if(widget->isMotionGridDisplayed() == m_requiredGridDisplayValue)
                return Qn::EnabledAction;
        } else {
            return Qn::EnabledAction;
        }
    }

    return Qn::InvisibleAction;
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
    foreach(const Item &item, sequence) {
        bool matches = checkOne(item);

        if(matches && m_matchMode == Qn::Any)
            return true;

        if(!matches && m_matchMode == Qn::All)
            return false;
    }

    if(m_matchMode == Qn::Any) {
        return false;
    } else /* if(m_matchMode == Qn::All) */ {
        return true;
    }
}

bool QnResourceActionCondition::checkOne(const QnResourcePtr &resource) {
    return m_criterion.check(resource) == QnResourceCriterion::Accept;
}

bool QnResourceActionCondition::checkOne(QnResourceWidget *widget) {
    QnResourcePtr resource = QnActionTargetTypes::resource(widget);
    return resource ? checkOne(resource) : false;
}


Qn::ActionVisibility QnResourceRemovalActionCondition::check(const QnResourceList &resources) {
    foreach(const QnResourcePtr &resource, resources) {
        if(!resource)
            continue; /* OK to remove. */

        if(resource->checkFlags(QnResource::layout))
            continue; /* OK to remove. */

        if(resource->checkFlags(QnResource::user))
            continue; /* OK to remove. */

        if(resource->checkFlags(QnResource::remote_server) || resource->checkFlags(QnResource::live_cam)) // TODO: move this to permissions.
            if(resource->getStatus() == QnResource::Offline)
                continue; /* Can remove only if offline. */

        return Qn::InvisibleAction;
    }

    return Qn::EnabledAction;
}


Qn::ActionVisibility QnLayoutItemRemovalActionCondition::check(const QnLayoutItemIndexList &layoutItems) {
    foreach(const QnLayoutItemIndex &item, layoutItems)
        if(!(accessController()->permissions(item.layout()) & Qn::WritePermission))
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


Qn::ActionVisibility QnLayoutCountActionCondition::check(const QnWorkbenchLayoutList &layouts) {
    if(workbench()->layouts().size() < m_requiredCount)
        return Qn::DisabledAction;

    return Qn::EnabledAction;
}


Qn::ActionVisibility QnTakeScreenshotActionCondition::check(const QnResourceWidgetList &widgets) {
    if(widgets.size() != 1)
        return Qn::InvisibleAction;

    Qn::RenderStatus renderStatus = widgets[0]->currentRenderStatus();
    if(renderStatus == Qn::NothingRendered || renderStatus == Qn::CannotRender)
        return Qn::DisabledAction;

    return Qn::EnabledAction;
}

