// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "action_parameters.h"

#include <core/resource/resource.h>
#include <core/resource/videowall_item_index.h>
#include <core/resource/videowall_matrix_index.h>
#include <nx/utils/log/log.h>
#include <nx/utils/range_adapters.h>
#include <nx/vms/client/desktop/resource/layout_item_index.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/workbench/workbench_layout.h>

namespace nx::vms::client::desktop {
namespace ui {
namespace action {

namespace {

bool checkType(const QVariant& items)
{
    ActionParameterType type = ParameterTypes::type(items);
    NX_ASSERT(type != OtherType, nx::format("Unrecognized action parameter type '%1'.").arg(items.typeName()));
    return type != OtherType;
}

} // namespace

Parameters::Parameters(const ArgumentHash& arguments)
{
    init(QVariant::fromValue<QnResourceList>(QnResourceList()), arguments);
}

Parameters::Parameters(const QVariant& items, const ArgumentHash& arguments)
{
    init(items, arguments);
}

Parameters::Parameters(const QList<QGraphicsItem*>& items, const ArgumentHash& arguments)
{
    init(QVariant::fromValue<QnResourceWidgetList>(ParameterTypes::widgets(items)), arguments);
}

Parameters::Parameters(QnResourceWidget* widget, const ArgumentHash& arguments)
{
    init(QVariant::fromValue<QnResourceWidget*>(widget), arguments);
}

Parameters::Parameters(const QnResourceWidgetList& widgets, const ArgumentHash& arguments)
{
    init(QVariant::fromValue<QnResourceWidgetList>(widgets), arguments);
}

Parameters::Parameters(const QnWorkbenchLayoutList& layouts, const ArgumentHash& arguments)
{
    init(QVariant::fromValue<QnWorkbenchLayoutList>(layouts), arguments);
}

Parameters::Parameters(const LayoutItemIndexList& layoutItems, const ArgumentHash& arguments)
{
    init(QVariant::fromValue<LayoutItemIndexList>(layoutItems), arguments);
}

Parameters::Parameters(const QnVideoWallItemIndexList& videoWallItems,
    const ArgumentHash& arguments)
{
    init(QVariant::fromValue<QnVideoWallItemIndexList>(videoWallItems), arguments);
}

Parameters::Parameters(const QnVideoWallMatrixIndexList& videoWallMatrices,
    const ArgumentHash& arguments)
{
    init(QVariant::fromValue<QnVideoWallMatrixIndexList>(videoWallMatrices), arguments);
}

void Parameters::setArguments(const ArgumentHash& arguments)
{
    for (ArgumentHash::const_iterator pos = arguments.begin(); pos != arguments.end(); pos++)
        setArgument(pos.key(), pos.value());
}

void Parameters::setArgument(int key, const QVariant& value)
{
    if (key == -1 && !checkType(value))
        return;

    m_arguments[key] = value;
}

void Parameters::setItems(const QVariant& items)
{
    setArgument(-1, items);
}

void Parameters::setResources(const QnResourceList& resources)
{
    setItems(QVariant::fromValue<QnResourceList>(resources));
}

ActionParameterType Parameters::type(int key) const
{
    return ParameterTypes::type(argument(key));
}

int Parameters::size(int key) const
{
    return ParameterTypes::size(argument(key));
}

QnResourceList Parameters::resources(int key) const
{
    return ParameterTypes::resources(argument(key));
}

QnResourcePtr Parameters::resource(int key) const
{
    QnResourceList resources = this->resources(key);

    if (resources.size() != 1)
        NX_WARNING(this, "Invalid number of target resources: expected %2, got %3.", 1, resources.size());

    return resources.isEmpty() ? QnResourcePtr() : resources.front();
}

LayoutItemIndexList Parameters::layoutItems(int key) const
{
    return ParameterTypes::layoutItems(argument(key));
}

QnVideoWallItemIndexList Parameters::videoWallItems(int key) const
{
    return ParameterTypes::videoWallItems(argument(key));
}

QnVideoWallMatrixIndexList Parameters::videoWallMatrices(int key) const
{
    return ParameterTypes::videoWallMatrices(argument(key));
}

QnWorkbenchLayoutList Parameters::layouts(int key) const
{
    return ParameterTypes::layouts(argument(key));
}

QnResourceWidget* Parameters::widget(int key) const
{
    QnResourceWidgetList widgets = this->widgets(key);

    if (widgets.size() != 1)
        NX_WARNING(this, "Invalid number of target widgets: expected %2, got %3.", 1, widgets.size());

    return widgets.isEmpty() ? nullptr : widgets.front();
}

QnResourceWidgetList Parameters::widgets(int key) const
{
    return ParameterTypes::widgets(argument(key));
}

ActionScopes Parameters::scope() const
{
    return m_scope;
}

void Parameters::setScope(ActionScopes scope)
{
    m_scope = scope;
}

void Parameters::init(const QVariant& items, const ArgumentHash& arguments)
{
    setArguments(arguments);
    setItems(items);
}

QString Parameters::toString() const
{
    QStringList arguments;
    if (m_arguments.contains(-1) && ParameterTypes::size(items()) > 0)
    {
        arguments.push_back("Items: [");
        arguments.push_back("    " + ParameterTypes::toString(items()));
        arguments.push_back("]");
    }

    for (auto [key, value]: nx::utils::constKeyValueRange(m_arguments))
    {
        if (key == -1)
            continue;

        arguments.push_back(nx::format("%1: [%2]").args(key, m_arguments[key]));
    }

    return arguments.empty() ? "[]" : arguments.join('\n');
}

} // namespace action
} // namespace ui
} // namespace nx::vms::client::desktop

