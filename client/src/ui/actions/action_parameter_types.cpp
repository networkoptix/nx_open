﻿#include "action_parameter_types.h"

#include <utils/common/flat_map.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/video_server_resource.h>
#include <core/resourcemanagment/resource_pool.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>

namespace ParameterMetaType {
    enum Type {
        ResourcePtr,
        UserResourcePtr,
        LayoutResourcePtr,
        VideoServerResourcePtr,
        ResourceList,
        ResourceWidget,
        ResourceWidgetList,
        LayoutItemIndexList,
        WorkbenchLayout,
        WorkbenchLayoutList,
        Invalid = -1
    };

} // namespace ParameterMetaType

namespace {
    typedef QnFlatMap<int, ParameterMetaType::Type, QnValueConstructor<ParameterMetaType::Type, ParameterMetaType::Invalid> > ParameterMetaTypeMap;

    Q_GLOBAL_STATIC_WITH_INITIALIZER(ParameterMetaTypeMap, qn_actionMetaTypeMap, {
        using namespace ParameterMetaType;

        x->insert(qMetaTypeId<QnResourcePtr>(),             ResourcePtr);
        x->insert(qMetaTypeId<QnUserResourcePtr>(),         UserResourcePtr);
        x->insert(qMetaTypeId<QnLayoutResourcePtr>(),       LayoutResourcePtr);
        x->insert(qMetaTypeId<QnVideoServerResourcePtr>(),  VideoServerResourcePtr);
        x->insert(qMetaTypeId<QnResourceList>(),            ResourceList);
        x->insert(qMetaTypeId<QnResourceWidget *>(),        ResourceWidget);
        x->insert(qMetaTypeId<QnResourceWidgetList>(),      ResourceWidgetList);
        x->insert(qMetaTypeId<QnLayoutItemIndexList>(),     LayoutItemIndexList);
        x->insert(qMetaTypeId<QnWorkbenchLayout *>(),       WorkbenchLayout);
        x->insert(qMetaTypeId<QnWorkbenchLayoutList>(),     WorkbenchLayoutList);
    });

    QnResourcePtr singleResource(const QVariant &items) {
        using namespace ParameterMetaType;

        switch(qn_actionMetaTypeMap()->value(items.userType())) {
        case ResourcePtr:
            return items.value<QnResourcePtr>();
        case UserResourcePtr:
            return items.value<QnUserResourcePtr>();
        case LayoutResourcePtr:
            return items.value<QnLayoutResourcePtr>();
        case VideoServerResourcePtr:
            return items.value<QnVideoServerResourcePtr>();
        default:
            return QnResourcePtr();
        }
    }


    template<class T>
    bool isValid(const T &value) {
        return value;
    }

    bool isValid(const QnLayoutItemIndex &value) {
        return !value.isNull();
    }

    template<class List, class T>
    List makeList(const T &value) {
        List result;
        if(isValid(value))
            result.push_back(value);
        return result;
    }

} // anonymous namespace

int QnActionParameterTypes::size(const QVariant &items) {
    using namespace ParameterMetaType;

    switch(qn_actionMetaTypeMap()->value(items.userType())) {
    case ResourcePtr:
    case UserResourcePtr:
    case LayoutResourcePtr:
    case VideoServerResourcePtr:
        return singleResource(items) ? 1 : 0;
    case ResourceList:
        return items.value<QnResourceList>().size();
    case ResourceWidget:
        return items.value<QnResourceWidget *>() ? 1 : 0;
    case ResourceWidgetList:
        return items.value<QnResourceWidgetList>().size();
    case LayoutItemIndexList:
        return items.value<QnLayoutItemIndexList>().size();
    case WorkbenchLayout:
        return items.value<QnWorkbenchLayout *>() ? 1 : 0;
    case WorkbenchLayoutList:
        return items.value<QnWorkbenchLayoutList>().size();
    default:
        return 0;
    }
}

Qn::ActionParameterType QnActionParameterTypes::type(const QVariant &items) {
    using namespace ParameterMetaType;

    switch(qn_actionMetaTypeMap()->value(items.userType())) {
    case ResourcePtr:
    case UserResourcePtr:
    case LayoutResourcePtr:
    case VideoServerResourcePtr:
    case ResourceList:
        return Qn::ResourceType;
    case ResourceWidget:
    case ResourceWidgetList:
        return Qn::WidgetType;
    case LayoutItemIndexList:
        return Qn::LayoutItemType;
    case WorkbenchLayout:
    case WorkbenchLayoutList:
        return Qn::LayoutType;
    default:
        return Qn::OtherType;
    }
}

QnResourcePtr QnActionParameterTypes::resource(QnResourceWidget *widget) {
    if(widget == NULL)
        return QnResourcePtr();

    return widget->resource();
}

QnResourceList QnActionParameterTypes::resources(QnResourceWidget *widget) {
    return makeList<QnResourceList>(resource(widget));
}

QnResourceList QnActionParameterTypes::resources(const QnResourceWidgetList &widgets) {
    QnResourceList result;

    foreach(QnResourceWidget *widget, widgets) {
        QnResourcePtr resource = QnActionParameterTypes::resource(widget);
        if(!resource)
            continue;

        result.push_back(resource);
    }

    return result;
}

QnResourceList QnActionParameterTypes::resources(const QnLayoutItemIndexList &layoutItems) {
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

QnResourcePtr QnActionParameterTypes::resource(QnWorkbenchLayout *layout) {
    if(layout == NULL)
        return QnResourcePtr();

    return layout->resource();
}

QnResourceList QnActionParameterTypes::resources(QnWorkbenchLayout *layout) {
    return makeList<QnResourceList>(resource(layout));
}

QnResourceList QnActionParameterTypes::resources(const QnWorkbenchLayoutList &layouts) {
    QnResourceList result;

    foreach(QnWorkbenchLayout *layout, layouts) {
        QnResourcePtr resource = QnActionParameterTypes::resource(layout);
        if(!resource)
            continue;

        result.push_back(resource);
    }

    return result;
}

QnResourceList QnActionParameterTypes::resources(const QVariant &items) {
    using namespace ParameterMetaType;

    switch(qn_actionMetaTypeMap()->value(items.userType())) {
    case ResourcePtr:
    case UserResourcePtr:
    case LayoutResourcePtr:
    case VideoServerResourcePtr:
        return makeList<QnResourceList>(singleResource(items));
    case ResourceList:
        return items.value<QnResourceList>();
    case ResourceWidget:
        return resources(items.value<QnResourceWidget *>());
    case ResourceWidgetList:
        return resources(items.value<QnResourceWidgetList>());
    case LayoutItemIndexList:
        return resources(items.value<QnLayoutItemIndexList>());
    case WorkbenchLayout:
        return resources(items.value<QnWorkbenchLayout *>());
    case WorkbenchLayoutList:
        return resources(items.value<QnWorkbenchLayoutList>());
    default:
        return QnResourceList();
    }
}

QnLayoutItemIndex QnActionParameterTypes::layoutItem(QnResourceWidget *widget) {
    QnWorkbenchLayout *layout = widget->item()->layout();
    if(layout == NULL)
        return QnLayoutItemIndex();

    QnLayoutResourcePtr resource = layout->resource();
    if(!resource) {
        qnWarning("Layout does not have an associated resource, item manipulation may not work as expected.");
        return QnLayoutItemIndex();
    }

    return QnLayoutItemIndex(resource, widget->item()->uuid());
}

QnLayoutItemIndexList QnActionParameterTypes::layoutItems(const QnResourceWidgetList &widgets) {
    QnLayoutItemIndexList result;
    
    foreach(QnResourceWidget *widget, widgets) {
        QnLayoutItemIndex layoutItem = QnActionParameterTypes::layoutItem(widget);
        if(!layoutItem.isNull())
            result.push_back(layoutItem);
    }
    
    return result;
}

QnLayoutItemIndexList QnActionParameterTypes::layoutItems(QnResourceWidget *widget) {
    return makeList<QnLayoutItemIndexList>(QnActionParameterTypes::layoutItem(widget));
}

QnLayoutItemIndexList QnActionParameterTypes::layoutItems(const QVariant &items) {
    using namespace ParameterMetaType;

    switch(qn_actionMetaTypeMap()->value(items.userType())) {
    case ResourceWidget:
        return layoutItems(items.value<QnResourceWidget *>());
    case ResourceWidgetList:
        return layoutItems(items.value<QnResourceWidgetList>());
    case LayoutItemIndexList:
        return items.value<QnLayoutItemIndexList>();
    default:
        return QnLayoutItemIndexList();
    }
}

QnWorkbenchLayoutList QnActionParameterTypes::layouts(const QVariant &items) {
    using namespace ParameterMetaType;

    switch(qn_actionMetaTypeMap()->value(items.userType())) {
    case WorkbenchLayout:
        return makeList<QnWorkbenchLayoutList>(items.value<QnWorkbenchLayout *>());
    case WorkbenchLayoutList:
        return items.value<QnWorkbenchLayoutList>();
    default:
        return QnWorkbenchLayoutList();
    }
}

QnResourceWidgetList QnActionParameterTypes::widgets(const QVariant &items) {
    using namespace ParameterMetaType;

    switch(qn_actionMetaTypeMap()->value(items.userType())) {
    case ResourceWidget: 
        return makeList<QnResourceWidgetList>(items.value<QnResourceWidget *>());
    case ResourceWidgetList:
        return items.value<QnResourceWidgetList>();
    default:
        return QnResourceWidgetList();
    }
}

QnResourceWidgetList QnActionParameterTypes::widgets(const QList<QGraphicsItem *> items) {
    QnResourceWidgetList result;

    foreach(QGraphicsItem *item, items) {
        QnResourceWidget *widget = item->isWidget() ? qobject_cast<QnResourceWidget *>(item->toGraphicsObject()) : NULL;
        if(widget == NULL)
            continue;

        result.push_back(widget);
    }

    return result;
}

