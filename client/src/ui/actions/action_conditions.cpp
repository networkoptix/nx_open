#include "action_conditions.h"

#include <utils/common/warnings.h>
#include <core/resourcemanagment/resource_criterion.h>

#include <ui/graphics/items/resource_widget.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>

#include "action_target_types.h"
#include "action_manager.h"

QnActionCondition::QnActionCondition(QObject *parent): 
    QObject(parent) 
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
        return Qn::InvisibleAction;
    }
}

void QnActionCondition::setManager(QnActionManager *manager) {
    m_manager = manager;
}

QnWorkbenchContext *QnActionCondition::context() const {
    return manager() ? manager()->context() : NULL;
}


Qn::ActionVisibility QnTargetlessActionCondition::check(const QVariant &items) {
    if(!items.isValid()) {
        return check(QnResourceList());
    } else {
        return base_type::check(items);
    }
}


Qn::ActionVisibility QnMotionGridDisplayActionCondition::check(const QnResourceWidgetList &widgets) {
    foreach(QGraphicsItem *item, widgets) {
        QnResourceWidget *widget = item->isWidget() ? qobject_cast<QnResourceWidget *>(item->toGraphicsObject()) : NULL;
        if(widget == NULL)
            continue;

        bool isCamera = widget->resource()->flags() & QnResource::network;
        if(!isCamera)
            continue;

        if(widget->isMotionGridDisplayed()) {
            if(m_condition == HasShownGrid)
                return Qn::VisibleAction;
        } else {
            if(m_condition == HasHiddenGrid)
                return Qn::VisibleAction;
        }
    }

    return Qn::InvisibleAction;
}


QnResourceActionCondition::QnResourceActionCondition(MatchMode matchMode, const QnResourceCriterion &criterion, QObject *parent): 
    QnActionCondition(parent),
    m_matchMode(matchMode),
    m_criterion(criterion)
{}

QnResourceActionCondition::~QnResourceActionCondition() {
    return;
}

Qn::ActionVisibility QnResourceActionCondition::check(const QnResourceList &resources) {
    return checkInternal<QnResourcePtr>(resources) ? Qn::VisibleAction : Qn::InvisibleAction;
}

Qn::ActionVisibility QnResourceActionCondition::check(const QnResourceWidgetList &widgets) {
    return checkInternal<QnResourceWidget *>(widgets) ? Qn::VisibleAction : Qn::InvisibleAction;
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


Qn::ActionVisibility QnResourceRemovalActionCondition::check(const QnResourceList &resources) {
    foreach(const QnResourcePtr &resource, resources) {
        if(!resource)
            continue; /* OK to remove. */

        if(QnUserResourcePtr user = resource.dynamicCast<QnUserResource>()) {
            if(!context()) 
                continue;

            if(user != context()->user())
                continue;

            return Qn::InvisibleAction; /* Can't delete self from server. */
        }

        if(resource->checkFlags(QnResource::layout))
            continue; /* OK to remove. */

        if(resource->checkFlags(QnResource::remote_server) || resource->checkFlags(QnResource::live_cam))
            if(resource->getStatus() == QnResource::Offline)
                continue; /* Can remove only if offline. */

        return Qn::InvisibleAction;
    }

    return Qn::VisibleAction;
}

Qn::ActionVisibility QnResourceSaveLayoutActionCondition::check(const QnResourceList &resources) {
    QnLayoutResourcePtr layout;

    if(m_current) {
        if(!context())
            return Qn::InvisibleAction;

        layout = context()->workbench()->currentLayout()->resource();
    } else {
        if(resources.size() != 1)
            return Qn::InvisibleAction;

        layout = resources[0].dynamicCast<QnLayoutResource>();
    }

    if(!layout)
        return Qn::InvisibleAction;

    if(context() && context()->snapshotManager()->isSaveable(layout)) {
        return Qn::VisibleAction;
    } else {
        return Qn::DisabledAction;
    }
}

Qn::ActionVisibility QnResourceActionUserAccessCondition::check(const QnResourceList &resources) {
    if(!context())
        return Qn::InvisibleAction;

    if(!context()->user())
        return Qn::InvisibleAction;

    if(m_adminRequired)
        return context()->user()->isAdmin() ? Qn::VisibleAction : Qn::InvisibleAction;

    return Qn::VisibleAction; 
}
