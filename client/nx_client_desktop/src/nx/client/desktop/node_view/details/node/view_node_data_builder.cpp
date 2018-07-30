#include "view_node_data_builder.h"

#include "view_node_constants.h"

namespace nx {
namespace client {
namespace desktop {
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
    m_data->setCommonNodeData(separatorCommonRole, true);
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
    const ColumnsSet& columns,
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

ViewNodeDataBuilder& ViewNodeDataBuilder::withSiblingGroup(int value)
{
    m_data->setCommonNodeData(siblingGroupCommonRole, value);
    return *this;
}

ViewNodeDataBuilder& ViewNodeDataBuilder::withExpanded(bool value)
{
    m_data->setCommonNodeData(expandedCommonRole, value);
    return *this;
}

ViewNodeData ViewNodeDataBuilder::data() const
{
    return *m_data;
}

} // namespace details
} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx

