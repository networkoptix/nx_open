#include "action_parameter_types.h"

#include <utils/common/flat_map.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_item_index.h>
#include <core/resource/videowall_item_index.h>
#include <core/resource/videowall_matrix_index.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>

namespace ParameterMetaType {
    enum Type {
        ResourcePtr,
        UserResourcePtr,
        LayoutResourcePtr,
        MediaServerResourcePtr,
        ResourceList,
        ResourceWidget,
        ResourceWidgetList,
        LayoutItemIndexList,
        VideoWallItemIndexList,
        VideoWallMatrixIndexList,
        WorkbenchLayout,
        WorkbenchLayoutList,
        Invalid = -1
    };

    template<Type type>
    struct ValueConstructor {
        Type operator()() const { return type; }
    };

} // namespace ParameterMetaType

namespace {
    class QnActionMetaTypeMap: public QnFlatMap<unsigned int, ParameterMetaType::Type, ParameterMetaType::ValueConstructor<ParameterMetaType::Invalid> > {
    public:
        QnActionMetaTypeMap() {
            using namespace ParameterMetaType;

            insert(qMetaTypeId<QnResourcePtr>(),             ResourcePtr);
            insert(qMetaTypeId<QnUserResourcePtr>(),         UserResourcePtr);
            insert(qMetaTypeId<QnLayoutResourcePtr>(),       LayoutResourcePtr);
            insert(qMetaTypeId<QnMediaServerResourcePtr>(),  MediaServerResourcePtr);
            insert(qMetaTypeId<QnResourceList>(),            ResourceList);
            insert(qMetaTypeId<QnResourceWidget *>(),        ResourceWidget);
            insert(qMetaTypeId<QnResourceWidgetList>(),      ResourceWidgetList);
            insert(qMetaTypeId<QnLayoutItemIndexList>(),     LayoutItemIndexList);
            insert(qMetaTypeId<QnVideoWallItemIndexList>(),  VideoWallItemIndexList);
            insert(qMetaTypeId<QnVideoWallMatrixIndexList>(),VideoWallMatrixIndexList);
            insert(qMetaTypeId<QnWorkbenchLayout *>(),       WorkbenchLayout);
            insert(qMetaTypeId<QnWorkbenchLayoutList>(),     WorkbenchLayoutList);
        }
    };
    Q_GLOBAL_STATIC(QnActionMetaTypeMap, qn_actionMetaTypeMap);

    QnResourcePtr singleResource(const QVariant &items) {
        using namespace ParameterMetaType;

        switch(qn_actionMetaTypeMap()->value(items.userType())) {
        case ResourcePtr:
            return items.value<QnResourcePtr>();
        case UserResourcePtr:
            return items.value<QnUserResourcePtr>();
        case LayoutResourcePtr:
            return items.value<QnLayoutResourcePtr>();
        case MediaServerResourcePtr:
            return items.value<QnMediaServerResourcePtr>();
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
    case MediaServerResourcePtr:
        return singleResource(items) ? 1 : 0;
    case ResourceList:
        return items.value<QnResourceList>().size();
    case ResourceWidget:
        return items.value<QnResourceWidget *>() ? 1 : 0;
    case ResourceWidgetList:
        return items.value<QnResourceWidgetList>().size();
    case LayoutItemIndexList:
        return items.value<QnLayoutItemIndexList>().size();
    case VideoWallItemIndexList:
        return items.value<QnVideoWallItemIndexList>().size();
    case VideoWallMatrixIndexList:
        return items.value<QnVideoWallMatrixIndexList>().size();
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
    case MediaServerResourcePtr:
    case ResourceList:
        return Qn::ResourceType;
    case ResourceWidget:
    case ResourceWidgetList:
        return Qn::WidgetType;
    case LayoutItemIndexList:
        return Qn::LayoutItemType;
    case VideoWallItemIndexList:
        return Qn::VideoWallItemType;
    case VideoWallMatrixIndexList:
        return Qn::VideoWallMatrixType;
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
        if(!data.resource.id.isNull()) {
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
    case MediaServerResourcePtr:
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

QnVideoWallItemIndexList QnActionParameterTypes::videoWallItems(const QVariant &items) {
    using namespace ParameterMetaType;

    switch(qn_actionMetaTypeMap()->value(items.userType())) {
    case VideoWallItemIndexList:
        return items.value<QnVideoWallItemIndexList>();
    default:
        return QnVideoWallItemIndexList();
    }
}

QnVideoWallMatrixIndexList QnActionParameterTypes::videoWallMatrices(const QVariant &items) {
    using namespace ParameterMetaType;

    switch(qn_actionMetaTypeMap()->value(items.userType())) {
    case VideoWallMatrixIndexList:
        return items.value<QnVideoWallMatrixIndexList>();
    default:
        return QnVideoWallMatrixIndexList();
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

