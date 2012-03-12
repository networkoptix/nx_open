#include "action_target_types.h"
#include <core/resource/resource_fwd.h>
#include <core/resource/layout_resource.h>
#include <core/resourcemanagment/resource_pool.h>
#include <ui/graphics/items/resource_widget.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include "ui/workbench/workbench_layout_synchronizer.h"

namespace {
    int qn_resource = 0;
    int qn_user = 0;
    int qn_layout = 0;
    int qn_server = 0;
    int qn_resourceList = 0;
    int qn_widgetList = 0;
    int qn_layoutItemList = 0;
    int qn_layoutList = 0;

    Q_GLOBAL_STATIC_WITH_INITIALIZER(bool, qn_initializeActionMetaTypes, {
        qn_resource         = qMetaTypeId<QnResourcePtr>();
        qn_user             = qMetaTypeId<QnUserResourcePtr>();
        qn_layout           = qMetaTypeId<QnLayoutResourcePtr>();
        qn_server           = qMetaTypeId<QnVideoServerResourcePtr>();
        qn_resourceList     = qMetaTypeId<QnResourceList>();
        qn_widgetList       = qMetaTypeId<QnResourceWidgetList>();
        qn_layoutItemList   = qMetaTypeId<QnLayoutItemIndexList>();
        qn_layoutList       = qMetaTypeId<QnWorkbenchLayoutList>();
    });

} // anonymous namespace

void QnActionTargetTypes::initialize() {
    qn_initializeActionMetaTypes();
}

int QnActionTargetTypes::size(const QVariant &items) {
    int t = items.userType();

    if(t == qn_resourceList) {
        return items.value<QnResourceList>().size();
    } else if(t == qn_widgetList) {
        return items.value<QnResourceWidgetList>().size();
    } else if(t == qn_layoutItemList) {
        return items.value<QnLayoutItemIndexList>().size();
    } else if(t == qn_layoutList) {
        return items.value<QnWorkbenchLayoutList>().size();
    } else if(t == qn_resource || t == qn_user || t == qn_layout || t == qn_server) {
        return 1;
    } else {
        return 0;
    }
}

Qn::ActionTargetType QnActionTargetTypes::type(const QVariant &items) {
    int t = items.userType();

    if(t == qn_resourceList) {
        return Qn::ResourceType;
    } else if(t == qn_widgetList) {
        return Qn::WidgetType;
    } else if(t == qn_layoutItemList) {
        return Qn::LayoutItemType;
    } else if(t == qn_layoutList) {
        return Qn::LayoutType;
    } else if(t == qn_resource || t == qn_user || t == qn_layout || t == qn_server) {
        return Qn::ResourceType;
    } else {
        return static_cast<Qn::ActionTargetType>(0);
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
    int t = items.userType();

    if(t == qn_resourceList) {
        return items.value<QnResourceList>();
    } else if(t == qn_widgetList) {
        return resources(items.value<QnResourceWidgetList>());
    } else if(t == qn_layoutItemList) {
        return resources(items.value<QnLayoutItemIndexList>());
    } else if(t == qn_layoutList) {
        return resources(items.value<QnWorkbenchLayoutList>());
    } else if(t == qn_resource || t == qn_user || t == qn_layout || t == qn_server) {
        QnResourceList result;
        if(t == qn_resource) {
            result.push_back(items.value<QnResourcePtr>());
        } else if(t == qn_user) {
            result.push_back(items.value<QnUserResourcePtr>());
        } else if(t == qn_layout) {
            result.push_back(items.value<QnLayoutResourcePtr>());
        } else /* if(t == qn_server) */ {
            result.push_back(items.value<QnVideoServerResourcePtr>());
        }
        return result;
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
            qnWarning("Appserver is down, layout item deletion may not work as expected.");

        result.push_back(QnLayoutItemIndex(resource, widget->item()->uuid()));
    }
    
    return result;
}

QnLayoutItemIndexList QnActionTargetTypes::layoutItems(const QVariant &items) {
    if(items.userType() == qn_layoutItemList) {
        return items.value<QnLayoutItemIndexList>();
    } else if(items.userType() == qn_widgetList) {
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
    if(items.userType() == qn_widgetList) {
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

