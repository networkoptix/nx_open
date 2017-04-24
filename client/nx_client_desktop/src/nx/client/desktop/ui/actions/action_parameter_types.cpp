#include "action_parameter_types.h"

#include <nx/utils/flat_map.h>

#include <common/common_module.h>
#include <client_core/client_core_module.h>

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

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace action {

namespace ParameterMetaType {
enum Type
{
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
struct ValueConstructor
{
    Type operator()() const { return type; }
};

} // namespace ParameterMetaType

namespace {
class QnActionMetaTypeMap: public QnFlatMap<unsigned int, ParameterMetaType::Type, ParameterMetaType::ValueConstructor<ParameterMetaType::Invalid> >
{
public:
    QnActionMetaTypeMap()
    {
        using namespace ParameterMetaType;

        insert(qMetaTypeId<QnResourcePtr>(), ResourcePtr);
        insert(qMetaTypeId<QnUserResourcePtr>(), UserResourcePtr);
        insert(qMetaTypeId<QnLayoutResourcePtr>(), LayoutResourcePtr);
        insert(qMetaTypeId<QnMediaServerResourcePtr>(), MediaServerResourcePtr);
        insert(qMetaTypeId<QnResourceList>(), ResourceList);
        insert(qMetaTypeId<QnResourceWidget*>(), ResourceWidget);
        insert(qMetaTypeId<QnResourceWidgetList>(), ResourceWidgetList);
        insert(qMetaTypeId<QnLayoutItemIndexList>(), LayoutItemIndexList);
        insert(qMetaTypeId<QnVideoWallItemIndexList>(), VideoWallItemIndexList);
        insert(qMetaTypeId<QnVideoWallMatrixIndexList>(), VideoWallMatrixIndexList);
        insert(qMetaTypeId<QnWorkbenchLayout*>(), WorkbenchLayout);
        insert(qMetaTypeId<QnWorkbenchLayoutList>(), WorkbenchLayoutList);
    }
};
Q_GLOBAL_STATIC(QnActionMetaTypeMap, qn_actionMetaTypeMap);

QnResourcePtr singleResource(const QVariant& items)
{
    using namespace ParameterMetaType;

    switch (qn_actionMetaTypeMap()->value(items.userType()))
    {
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
bool isValid(const T& value)
{
    return value;
}

bool isValid(const QnLayoutItemIndex& value)
{
    return !value.isNull();
}

template<class List, class T>
List makeList(const T& value)
{
    List result;
    if (isValid(value))
        result.push_back(value);
    return result;
}

} // anonymous namespace

int ParameterTypes::size(const QVariant& items)
{
    using namespace ParameterMetaType;

    switch (qn_actionMetaTypeMap()->value(items.userType()))
    {
        case ResourcePtr:
        case UserResourcePtr:
        case LayoutResourcePtr:
        case MediaServerResourcePtr:
            return singleResource(items) ? 1 : 0;
        case ResourceList:
            return items.value<QnResourceList>().size();
        case ResourceWidget:
            return items.value<QnResourceWidget*>() ? 1 : 0;
        case ResourceWidgetList:
            return items.value<QnResourceWidgetList>().size();
        case LayoutItemIndexList:
            return items.value<QnLayoutItemIndexList>().size();
        case VideoWallItemIndexList:
            return items.value<QnVideoWallItemIndexList>().size();
        case VideoWallMatrixIndexList:
            return items.value<QnVideoWallMatrixIndexList>().size();
        case WorkbenchLayout:
            return items.value<QnWorkbenchLayout*>() ? 1 : 0;
        case WorkbenchLayoutList:
            return items.value<QnWorkbenchLayoutList>().size();
        default:
            return 0;
    }
}

ActionParameterType ParameterTypes::type(const QVariant& items)
{
    using namespace ParameterMetaType;

    switch (qn_actionMetaTypeMap()->value(items.userType()))
    {
        case ResourcePtr:
        case UserResourcePtr:
        case LayoutResourcePtr:
        case MediaServerResourcePtr:
        case ResourceList:
            return ResourceType;
        case ResourceWidget:
        case ResourceWidgetList:
            return WidgetType;
        case LayoutItemIndexList:
            return LayoutItemType;
        case VideoWallItemIndexList:
            return VideoWallItemType;
        case VideoWallMatrixIndexList:
            return VideoWallMatrixType;
        case WorkbenchLayout:
        case WorkbenchLayoutList:
            return LayoutType;
        default:
            return OtherType;
    }
}

QnResourcePtr ParameterTypes::resource(QnResourceWidget* widget)
{
    if (widget == NULL)
        return QnResourcePtr();

    return widget->resource();
}

QnResourceList ParameterTypes::resources(QnResourceWidget* widget)
{
    return makeList<QnResourceList>(resource(widget));
}

QnResourceList ParameterTypes::resources(const QnResourceWidgetList& widgets)
{
    QnResourceList result;

    for (auto widget: widgets)
    {
        QnResourcePtr resource = ParameterTypes::resource(widget);
        if (!resource)
            continue;

        result.push_back(resource);
    }

    return result;
}

QnResourceList ParameterTypes::resources(const QnLayoutItemIndexList& layoutItems)
{
    QnResourceList result;

    for (const auto& index: layoutItems)
    {
        if (index.isNull())
            continue;

        QnLayoutItemData data = index.layout()->getItem(index.uuid());

        QnResourcePtr resource = qnClientCoreModule->commonModule()->
            resourcePool()->getResourceByDescriptor(data.resource);
        if (resource)
            result.push_back(resource);
    }

    return result;
}

QnResourcePtr ParameterTypes::resource(QnWorkbenchLayout* layout)
{
    if (layout == NULL)
        return QnResourcePtr();

    return layout->resource();
}

QnResourceList ParameterTypes::resources(QnWorkbenchLayout* layout)
{
    return makeList<QnResourceList>(resource(layout));
}

QnResourceList ParameterTypes::resources(const QnWorkbenchLayoutList& layouts)
{
    QnResourceList result;

    for (auto layout: layouts)
    {
        QnResourcePtr resource = ParameterTypes::resource(layout);
        if (!resource)
            continue;

        result.push_back(resource);
    }

    return result;
}

QnResourceList ParameterTypes::resources(const QVariant& items)
{
    using namespace ParameterMetaType;

    switch (qn_actionMetaTypeMap()->value(items.userType()))
    {
        case ResourcePtr:
        case UserResourcePtr:
        case LayoutResourcePtr:
        case MediaServerResourcePtr:
            return makeList<QnResourceList>(singleResource(items));
        case ResourceList:
            return items.value<QnResourceList>();
        case ResourceWidget:
            return resources(items.value<QnResourceWidget*>());
        case ResourceWidgetList:
            return resources(items.value<QnResourceWidgetList>());
        case LayoutItemIndexList:
            return resources(items.value<QnLayoutItemIndexList>());
        case WorkbenchLayout:
            return resources(items.value<QnWorkbenchLayout*>());
        case WorkbenchLayoutList:
            return resources(items.value<QnWorkbenchLayoutList>());
        default:
            return QnResourceList();
    }
}

QnLayoutItemIndex ParameterTypes::layoutItem(QnResourceWidget* widget)
{
    QnWorkbenchLayout* layout = widget->item()->layout();
    if (layout == NULL)
        return QnLayoutItemIndex();

    QnLayoutResourcePtr resource = layout->resource();
    if (!resource)
    {
        qnWarning("Layout does not have an associated resource, item manipulation may not work as expected.");
        return QnLayoutItemIndex();
    }

    return QnLayoutItemIndex(resource, widget->item()->uuid());
}

QnLayoutItemIndexList ParameterTypes::layoutItems(const QnResourceWidgetList& widgets)
{
    QnLayoutItemIndexList result;

    for (auto widget: widgets)
    {
        QnLayoutItemIndex layoutItem = ParameterTypes::layoutItem(widget);
        if (!layoutItem.isNull())
            result.push_back(layoutItem);
    }

    return result;
}

QnLayoutItemIndexList ParameterTypes::layoutItems(QnResourceWidget* widget)
{
    return makeList<QnLayoutItemIndexList>(ParameterTypes::layoutItem(widget));
}

QnLayoutItemIndexList ParameterTypes::layoutItems(const QVariant& items)
{
    using namespace ParameterMetaType;

    switch (qn_actionMetaTypeMap()->value(items.userType()))
    {
        case ResourceWidget:
            return layoutItems(items.value<QnResourceWidget*>());
        case ResourceWidgetList:
            return layoutItems(items.value<QnResourceWidgetList>());
        case LayoutItemIndexList:
            return items.value<QnLayoutItemIndexList>();
        default:
            return QnLayoutItemIndexList();
    }
}

QnVideoWallItemIndexList ParameterTypes::videoWallItems(const QVariant& items)
{
    using namespace ParameterMetaType;

    switch (qn_actionMetaTypeMap()->value(items.userType()))
    {
        case VideoWallItemIndexList:
            return items.value<QnVideoWallItemIndexList>();
        default:
            return QnVideoWallItemIndexList();
    }
}

QnVideoWallMatrixIndexList ParameterTypes::videoWallMatrices(const QVariant& items)
{
    using namespace ParameterMetaType;

    switch (qn_actionMetaTypeMap()->value(items.userType()))
    {
        case VideoWallMatrixIndexList:
            return items.value<QnVideoWallMatrixIndexList>();
        default:
            return QnVideoWallMatrixIndexList();
    }
}

QnWorkbenchLayoutList ParameterTypes::layouts(const QVariant& items)
{
    using namespace ParameterMetaType;

    switch (qn_actionMetaTypeMap()->value(items.userType()))
    {
        case WorkbenchLayout:
            return makeList<QnWorkbenchLayoutList>(items.value<QnWorkbenchLayout*>());
        case WorkbenchLayoutList:
            return items.value<QnWorkbenchLayoutList>();
        default:
            return QnWorkbenchLayoutList();
    }
}

QnResourceWidgetList ParameterTypes::widgets(const QVariant& items)
{
    using namespace ParameterMetaType;

    switch (qn_actionMetaTypeMap()->value(items.userType()))
    {
        case ResourceWidget:
            return makeList<QnResourceWidgetList>(items.value<QnResourceWidget*>());
        case ResourceWidgetList:
            return items.value<QnResourceWidgetList>();
        default:
            return QnResourceWidgetList();
    }
}

QnResourceWidgetList ParameterTypes::widgets(const QList<QGraphicsItem*>& items)
{
    QnResourceWidgetList result;

    for (auto item: items)
    {
        const auto widget = item->isWidget()
            ? qobject_cast<QnResourceWidget*>(item->toGraphicsObject())
            : nullptr;

        if (!widget)
            continue;

        result.push_back(widget);
    }

    return result;
}

} // namespace action
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx

