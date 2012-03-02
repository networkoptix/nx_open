#include "action_conditions.h"

#include <utils/common/warnings.h>
#include <core/resourcemanagment/resource_criterion.h>

#include <ui/graphics/items/resource_widget.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>

#include "action_target_types.h"

QnActionCondition::QnActionCondition(QObject *parent): 
    QObject(parent) 
{
    QnActionTargetTypes::initialize();
}

bool QnActionCondition::check(const QnResourceList &resources) { 
    Q_UNUSED(resources);

    return false; 
};

bool QnActionCondition::check(const QnLayoutItemIndexList &layoutItems) { 
    return check(QnActionTargetTypes::resources(layoutItems));
};

bool QnActionCondition::check(const QnResourceWidgetList &widgets) { 
    return check(QnActionTargetTypes::layoutItems(widgets));
};

bool QnActionCondition::check(const QnWorkbenchLayoutList &layouts) { 
    return check(QnActionTargetTypes::resources(layouts));
};


bool QnActionCondition::check(const QVariant &items) {
    if(items.userType() == QnActionTargetTypes::resourceList()) {
        return check(items.value<QnResourceList>());
    } else if(items.userType() == QnActionTargetTypes::widgetList()) {
        return check(items.value<QnResourceWidgetList>());
    } else if(items.userType() == QnActionTargetTypes::layoutList()) {
        return check(items.value<QnWorkbenchLayoutList>());
    } else if(items.userType() == QnActionTargetTypes::layoutItemList()) {
        return check(items.value<QnLayoutItemIndexList>());
    } else {
        qnWarning("Invalid action condition parameter type '%1'.", items.typeName());
        return false;
    }
}


bool QnMotionGridDisplayActionCondition::check(const QnResourceWidgetList &widgets) {
    foreach(QGraphicsItem *item, widgets) {
        QnResourceWidget *widget = item->isWidget() ? qobject_cast<QnResourceWidget *>(item->toGraphicsObject()) : NULL;
        if(widget == NULL)
            continue;

        bool isCamera = widget->resource()->flags() & QnResource::network;
        if(!isCamera)
            continue;

        if(widget->isMotionGridDisplayed()) {
            if(m_condition == HasShownGrid)
                return true;
        } else {
            if(m_condition == HasHiddenGrid)
                return true;
        }
    }

    return false;
}


QnResourceActionCondition::QnResourceActionCondition(MatchMode matchMode, const QnResourceCriterion &criterion, QObject *parent): 
    QnActionCondition(parent),
    m_matchMode(matchMode),
    m_criterion(criterion)
{}

QnResourceActionCondition::~QnResourceActionCondition() {
    return;
}

bool QnResourceActionCondition::check(const QnResourceList &resources) {
    return checkInternal<QnResourcePtr>(resources);
}

bool QnResourceActionCondition::check(const QnResourceWidgetList &widgets) {
    return checkInternal<QnResourceWidget *>(widgets);
}

template<class Item, class ItemSequence>
bool QnResourceActionCondition::checkInternal(const ItemSequence &sequence) {
    foreach(const Item &item, sequence) {
        bool matches = checkOne(item);

        if(matches && m_matchMode == OneMatches)
            return true;

        if(!matches && m_matchMode == AllMatch)
            return false;
    }

    if(m_matchMode == OneMatches) {
        return false;
    } else /* if(m_matchMode == AllMatch) */ {
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


bool QnResourceRemovalActionCondition::check(const QnResourceList &resources) {
    foreach(const QnResourcePtr &resource, resources) {
        if(!resource)
            continue; /* OK to remove. */

        if(resource->checkFlags(QnResource::user) || resource->checkFlags(QnResource::layout))
            continue; /* OK to remove. */

        if(resource->checkFlags(QnResource::remote_server) || resource->checkFlags(QnResource::live_cam))
            if(resource->getStatus() == QnResource::Offline)
                continue; /* Can remove only if offline. */

        return false;
    }

    return true;
}

