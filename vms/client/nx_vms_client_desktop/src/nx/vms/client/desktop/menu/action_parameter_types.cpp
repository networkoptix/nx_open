// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "action_parameter_types.h"

#include <core/resource/media_server_resource.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_item_index.h>
#include <core/resource/videowall_matrix_index.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/flat_map.h>
#include <nx/vms/client/desktop/resource/layout_item_index.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource/resource_descriptor.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>

namespace nx::vms::client::desktop {
namespace menu {

namespace ParameterMetaType {
enum Type
{
    ResourcePtr,
    UserResourcePtr,
    QnLayoutResourcePtrMetaType,
    LayoutResourcePtrMetaType,
    MediaServerResourcePtr,
    ResourceList,
    ResourceWidget,
    ResourceWidgetList,
    LayoutItemIndexListMetaType,
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
        insert(qMetaTypeId<QnLayoutResourcePtr>(), QnLayoutResourcePtrMetaType);
        insert(qMetaTypeId<LayoutResourcePtr>(), LayoutResourcePtrMetaType);
        insert(qMetaTypeId<QnMediaServerResourcePtr>(), MediaServerResourcePtr);
        insert(qMetaTypeId<QnResourceList>(), ResourceList);
        insert(qMetaTypeId<QnResourceWidget*>(), ResourceWidget);
        insert(qMetaTypeId<QnResourceWidgetList>(), ResourceWidgetList);
        insert(qMetaTypeId<LayoutItemIndexList>(), LayoutItemIndexListMetaType);
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
        case QnLayoutResourcePtrMetaType:
            return items.value<QnLayoutResourcePtr>();
        case LayoutResourcePtrMetaType:
            return items.value<LayoutResourcePtr>();
        case MediaServerResourcePtr:
            return items.value<QnMediaServerResourcePtr>();
        default:
            return QnResourcePtr();
    }
}


template<class T>
bool isValid(const T& value)
{
    return (bool) value;
}

bool isValid(const LayoutItemIndex& value)
{
    return !value.isNull();
}

bool isValid(const QnUuid& value)
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

template<class List>
QnResourceList getVideowalls(const List& items)
{
    QSet<QnResourcePtr> result;
    for (const auto& item: items)
    {
        if (auto videowall = item.videowall())
            result.insert(std::move(videowall));
    }

    return QnResourceList(
        std::make_move_iterator(result.begin()),
        std::make_move_iterator(result.end()));
}

} // anonymous namespace

int ParameterTypes::size(const QVariant& items)
{
    using namespace ParameterMetaType;

    switch (qn_actionMetaTypeMap()->value(items.userType()))
    {
        case ResourcePtr:
        case UserResourcePtr:
        case QnLayoutResourcePtrMetaType:
        case LayoutResourcePtrMetaType:
        case MediaServerResourcePtr:
            return singleResource(items) ? 1 : 0;
        case ResourceList:
            return items.value<QnResourceList>().size();
        case ResourceWidget:
            return items.value<QnResourceWidget*>() ? 1 : 0;
        case ResourceWidgetList:
            return items.value<QnResourceWidgetList>().size();
        case LayoutItemIndexListMetaType:
            return items.value<LayoutItemIndexList>().size();
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
        case QnLayoutResourcePtrMetaType:
        case LayoutResourcePtrMetaType:
        case MediaServerResourcePtr:
        case ResourceList:
            return ResourceType;
        case ResourceWidget:
        case ResourceWidgetList:
            return WidgetType;
        case LayoutItemIndexListMetaType:
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
    if (widget == nullptr)
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

QnResourceList ParameterTypes::resources(const LayoutItemIndexList& layoutItems)
{
    QnResourceList result;

    for (const auto& index: layoutItems)
    {
        if (index.isNull())
            continue;

        common::LayoutItemData data = index.layout()->getItem(index.uuid());
        if (auto resource = getResourceByDescriptor(data.resource))
            result.push_back(resource);
    }

    return result;
}

QnResourcePtr ParameterTypes::resource(QnWorkbenchLayout* layout)
{
    if (layout == nullptr)
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

QnResourceList ParameterTypes::videowalls(const QVariant& items)
{
    using namespace ParameterMetaType;

    switch (qn_actionMetaTypeMap()->value(items.userType()))
    {
        case VideoWallItemIndexList:
            return videowalls(items.value<QnVideoWallItemIndexList>());
        case VideoWallMatrixIndexList:
            return videowalls(items.value<QnVideoWallMatrixIndexList>());
        default:
            return resources(items).filtered<QnVideoWallResource>();
    }
}

QnResourceList ParameterTypes::videowalls(const QnVideoWallItemIndexList& items)
{
    return getVideowalls(items);
}

QnResourceList ParameterTypes::videowalls(const QnVideoWallMatrixIndexList& matrices)
{
    return getVideowalls(matrices);
}

QnResourceList ParameterTypes::resources(const QVariant& items)
{
    using namespace ParameterMetaType;

    switch (qn_actionMetaTypeMap()->value(items.userType()))
    {
        case ResourcePtr:
        case UserResourcePtr:
        case QnLayoutResourcePtrMetaType:
        case LayoutResourcePtrMetaType:
        case MediaServerResourcePtr:
            return makeList<QnResourceList>(singleResource(items));
        case ResourceList:
            return items.value<QnResourceList>();
        case ResourceWidget:
            return resources(items.value<QnResourceWidget*>());
        case ResourceWidgetList:
            return resources(items.value<QnResourceWidgetList>());
        case LayoutItemIndexListMetaType:
            return resources(items.value<LayoutItemIndexList>());
        case WorkbenchLayout:
            return resources(items.value<QnWorkbenchLayout*>());
        case WorkbenchLayoutList:
            return resources(items.value<QnWorkbenchLayoutList>());
        default:
            return QnResourceList();
    }
}

LayoutItemIndex ParameterTypes::layoutItem(QnResourceWidget* widget)
{
    // TODO: #sivanov Check is not required since 5.1.
    if (!NX_ASSERT(widget->item()))
        return {};

    QnWorkbenchLayout* layout = widget->item()->layout();
    if (layout == nullptr)
        return LayoutItemIndex();

    LayoutResourcePtr resource = layout->resource();
    if (!resource)
    {
        NX_ASSERT(false,
            "Layout does not have an associated resource, item manipulation may not work as expected.");
        return LayoutItemIndex();
    }

    return LayoutItemIndex(resource, widget->item()->uuid());
}

LayoutItemIndexList ParameterTypes::layoutItems(const QnResourceWidgetList& widgets)
{
    LayoutItemIndexList result;

    for (auto widget: widgets)
    {
        LayoutItemIndex layoutItem = ParameterTypes::layoutItem(widget);
        if (!layoutItem.isNull())
            result.push_back(layoutItem);
    }

    return result;
}

LayoutItemIndexList ParameterTypes::layoutItems(QnResourceWidget* widget)
{
    return makeList<LayoutItemIndexList>(ParameterTypes::layoutItem(widget));
}

LayoutItemIndexList ParameterTypes::layoutItems(const QVariant& items)
{
    using namespace ParameterMetaType;

    switch (qn_actionMetaTypeMap()->value(items.userType()))
    {
        case ResourceWidget:
            return layoutItems(items.value<QnResourceWidget*>());
        case ResourceWidgetList:
            return layoutItems(items.value<QnResourceWidgetList>());
        case LayoutItemIndexListMetaType:
            return items.value<LayoutItemIndexList>();
        default:
            return LayoutItemIndexList();
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

QString ParameterTypes::toString(const QVariant& items)
{
    using namespace ParameterMetaType;

    QStringList result;

    switch (qn_actionMetaTypeMap()->value(items.userType()))
    {
        case ResourcePtr:
            return nx::format("QnResourcePtr %1").arg(items.value<QnResourcePtr>());
        case UserResourcePtr:
            return nx::format("QnUserResourcePtr %1").arg(items.value<QnUserResourcePtr>());
        case QnLayoutResourcePtrMetaType:
            return nx::format("QnLayoutResourcePtr %1").arg(items.value<QnLayoutResourcePtr>());
        case LayoutResourcePtrMetaType:
            return nx::format("LayoutResourcePtr %1").arg(items.value<LayoutResourcePtr>());
        case MediaServerResourcePtr:
            return nx::format("QnMediaServerResourcePtr %1").arg(items.value<QnMediaServerResourcePtr>());
        case ResourceList:
            return nx::format("QnResourceList %1").container(items.value<QnResourceList>());
        case ResourceWidget:
            return nx::format("QnResourceWidget %1").arg(items.value<QnResourceWidget*>());
        case ResourceWidgetList:
            return nx::format("QnResourceWidgetList %1").container(items.value<QnResourceWidgetList>());
        case LayoutItemIndexListMetaType:
            return nx::format("LayoutItemIndexList %1").container(items.value<LayoutItemIndexList>());
        case VideoWallItemIndexList:
            return nx::format("QnVideoWallItemIndexList %1").container(items.value<QnVideoWallItemIndexList>());
        case VideoWallMatrixIndexList:
            return nx::format("QnVideoWallMatrixIndexList %1").container(items.value<QnVideoWallMatrixIndexList>());
        case WorkbenchLayout:
            return nx::format("QnWorkbenchLayout* %1").arg(items.value<QnWorkbenchLayout*>());
        case WorkbenchLayoutList:
            return nx::format("QnWorkbenchLayoutList %1").container(items.value<QnWorkbenchLayoutList>());
        default:
            return nx::format("Invalid type");
    }
}

} // namespace menu
} // namespace nx::vms::client::desktop
