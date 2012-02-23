#include "action_target_types.h"
#include <core/resource/resource_fwd.h>
#include <core/resource/layout_resource.h>
#include <core/resourcemanagment/resource_pool.h>
#include <ui/graphics/items/resource_widget.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>

namespace {
    int qn_widgetListMetaType = 0;
    int qn_resourceListMetaType = 0;
    int qn_layoutItemListMetaType = 0;
    int qn_layoutList = 0;

    Q_GLOBAL_STATIC_WITH_INITIALIZER(bool, qn_initializeActionMetaTypes, {
        qn_widgetListMetaType = qMetaTypeId<QnResourceWidgetList>();
        qn_resourceListMetaType = qMetaTypeId<QnResourceList>();
        qn_layoutItemListMetaType = qMetaTypeId<QnLayoutItemIndexList>();
        qn_layoutList = qMetaTypeId<QnWorkbenchLayoutList>();
    });

} // anonymous namespace

int QnActionTargetTypes::resourceList() {
    return qn_resourceListMetaType;
}

int QnActionTargetTypes::widgetList() {
    return qn_widgetListMetaType;
}

int QnActionTargetTypes::layoutItemList() {
    return qn_layoutItemListMetaType;
}

int QnActionTargetTypes::layoutList() {
    return qn_layoutList;
}

void QnActionTargetTypes::initialize() {
    qn_initializeActionMetaTypes();
}

int QnActionTargetTypes::size(const QVariant &items) {
    if(items.userType() == resourceList()) {
        return items.value<QnResourceList>().size();
    } else if(items.userType() == widgetList()) {
        return items.value<QnResourceWidgetList>().size();
    } else if(items.userType() == layoutItemList()) {
        return items.value<QnLayoutItemIndexList>().size();
    } else if(items.userType() == layoutList()) {
        return items.value<QnWorkbenchLayoutList>().size();
    } else {
        return 0;
    }
}

Qn::ActionTarget QnActionTargetTypes::target(const QVariant &items) {
    if(items.userType() == resourceList()) {
        return Qn::ResourceTarget;
    } else if(items.userType() == widgetList()) {
        return Qn::WidgetTarget;
    } else if(items.userType() == layoutItemList()) {
        return Qn::LayoutItemTarget;
    } else if(items.userType() == layoutList()) {
        return Qn::LayoutTarget;
    } else {
        return static_cast<Qn::ActionTarget>(0);
    }
}


QnResourcePtr QnActionTargetTypes::resource(QnResourceWidget *widget) {
    if(widget == NULL)
        return QnResourcePtr();

    return widget->resource();
}

QnResourceList QnActionTargetTypes::resources(const QnResourceWidgetList &widgets) {
    QnResourceList result;

    foreach(QnResourceWidget *widget, widgets) {
        QnResourcePtr resource = QnActionTargetTypes::resource(widget);
        if(!resource)
            continue;

        result.push_back(resource);
    }

    return result;
}

QnResourceList QnActionTargetTypes::resources(const QnLayoutItemIndexList &layoutItems) {
    QnResourceList result;

    foreach(const QnLayoutItemIndex &index, layoutItems) {
        if(index.isNull())
            continue;

        QnLayoutItemData data = index.layout()->getItem(index.uuid());
        
        QnResourcePtr resource;
        if(data.resource.id.isValid()) {
            resource = qnResPool->getResourceById(data.resource.id);
        } else {
            resource = qnResPool->getResourceByUniqId(data.resource.path);
        }

        if(resource)
            result.push_back(resource);
    }

    return result;
}

QnResourceList QnActionTargetTypes::resources(const QnWorkbenchLayoutList &layouts) {
    QnResourceList result;

    foreach(QnWorkbenchLayout *layout, layouts) {
        QnLayoutResourcePtr resource = layout->resource();
        if(!resource)
            continue;

        result.push_back(resource);
    }

    return result;
}

QnResourceList QnActionTargetTypes::resources(const QVariant &items) {
    if(items.userType() == qn_resourceListMetaType) {
        return items.value<QnResourceList>();
    } else if(items.userType() == qn_widgetListMetaType) {
        return resources(items.value<QnResourceWidgetList>());
    } else if(items.userType() == qn_layoutItemListMetaType) {
        return resources(items.value<QnLayoutItemIndexList>());
    } else if(items.userType() == qn_layoutList) {
        return resources(items.value<QnWorkbenchLayoutList>());
    } else {
        return QnResourceList();
    }
}

QnLayoutItemIndexList QnActionTargetTypes::layoutItems(const QnResourceWidgetList &widgets) {
    QnLayoutItemIndexList result;
    
    foreach(QnResourceWidget *widget, widgets) {
        QnWorkbenchLayout *layout = widget->item()->layout();
        if(layout == NULL)
            continue;

        QnLayoutResourcePtr resource = layout->resource();
        if(!resource)
            continue;

        result.push_back(QnLayoutItemIndex(resource, widget->item()->uuid()));
    }
    
    return result;
}

QnLayoutItemIndexList QnActionTargetTypes::layoutItems(const QVariant &items) {
    if(items.userType() == qn_layoutItemListMetaType) {
        return items.value<QnLayoutItemIndexList>();
    } else if(items.userType() == qn_widgetListMetaType) {
        return layoutItems(items.value<QnResourceWidgetList>());
    } else {
        return QnLayoutItemIndexList();
    }
}

QnWorkbenchLayoutList QnActionTargetTypes::layouts(const QVariant &items) {
    if(items.userType() == qn_layoutList) {
        return items.value<QnWorkbenchLayoutList>();
    } else {
        return QnWorkbenchLayoutList();
    }
}

QnResourceWidgetList QnActionTargetTypes::widgets(const QVariant &items) {
    if(items.userType() == qn_widgetListMetaType) {
        return items.value<QnResourceWidgetList>();
    } else {
        return QnResourceWidgetList();
    }
}

QnResourceWidgetList QnActionTargetTypes::widgets(const QList<QGraphicsItem *> items) {
    QnResourceWidgetList result;

    foreach(QGraphicsItem *item, items) {
        QnResourceWidget *widget = item->isWidget() ? qobject_cast<QnResourceWidget *>(item->toGraphicsObject()) : NULL;
        if(widget == NULL)
            continue;

        result.push_back(widget);
    }

    return result;
}

