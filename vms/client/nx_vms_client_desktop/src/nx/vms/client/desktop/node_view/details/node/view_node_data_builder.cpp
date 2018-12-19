#include "view_node_data_builder.h"

#include "view_node_constants.h"

namespace nx::vms::client::desktop {
namespace node_view {
namespace details {

ViewNodeDataBuilder::ViewNodeDataBuilder():
    m_outerData(false),
    m_data(new ViewNodeData())
{
}

ViewNodeDataBuilder::ViewNodeDataBuilder(ViewNodeData& data):
    m_outerData(true),
    m_data(&data)
{
}

ViewNodeDataBuilder::~ViewNodeDataBuilder()
{
    if (m_outerData)
        m_data.take();
}

ViewNodeDataBuilder& ViewNodeDataBuilder::separator()
{
    m_data->setProperty(isSeparatorProperty, true);
    return *this;
}

ViewNodeDataBuilder& ViewNodeDataBuilder::withText(int column, const QString& value)
{
    if (value.isEmpty())
        m_data->removeData(column, Qt::DisplayRole);
    else
        m_data->setData(column, Qt::DisplayRole, value);
    return *this;
}

ViewNodeDataBuilder& ViewNodeDataBuilder::withCheckedState(
    int column,
    Qt::CheckState value)
{
    m_data->setData(column, Qt::CheckStateRole, value);
    return *this;
}

ViewNodeDataBuilder& ViewNodeDataBuilder::withCheckedState(
    const ColumnSet& columns,
    Qt::CheckState value)
{
    for (const auto column: columns)
        m_data->setData(column, Qt::CheckStateRole, value);
    return *this;
}

ViewNodeDataBuilder& ViewNodeDataBuilder::withCheckedState(
    int column,
    const OptionalCheckedState& value)
{
    if (value)
        return withCheckedState(column, *value);

    m_data->removeData(column, Qt::CheckStateRole);
    return *this;
}

ViewNodeDataBuilder& ViewNodeDataBuilder::withIcon(int column, const QIcon& value)
{
    if (value.isNull())
        m_data->removeData(column, Qt::DecorationRole);
    else
        m_data->setData(column, Qt::DecorationRole, value);
    return *this;
}

ViewNodeDataBuilder& ViewNodeDataBuilder::withGroupSortOrder(int value)
{
    m_data->setProperty(groupSortOrderProperty, value);
    return *this;
}

ViewNodeDataBuilder& ViewNodeDataBuilder::withExpanded(bool value)
{
    m_data->setProperty(isExpandedProperty, value);
    return *this;
}

ViewNodeDataBuilder& ViewNodeDataBuilder::withData(int column, int role, const QVariant& value)
{
    m_data->setData(column, role, value);
    return *this;
}

ViewNodeDataBuilder& ViewNodeDataBuilder::withCommonData(int role, const QVariant& value)
{
    m_data->setProperty(role, value);
    return *this;
}

ViewNodeDataBuilder& ViewNodeDataBuilder::withFlag(int column, Qt::ItemFlag flag, bool value)
{
    m_data->setFlag(column, flag, value);
    return *this;
}

ViewNodeDataBuilder& ViewNodeDataBuilder::withFlags(int column, Qt::ItemFlags flags)
{
    m_data->setFlags(column, flags);
    return *this;
}

ViewNodeData ViewNodeDataBuilder::data() const
{
    return *m_data;
}

} // namespace details
} // namespace node_view
} // namespace nx::vms::client::desktop

