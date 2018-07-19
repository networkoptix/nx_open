#include "view_node_data_builder.h"

#include <nx/client/desktop/resource_views/node_view/node_view_constants.h>

namespace nx {
namespace client {
namespace desktop {

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
    m_data->setData(node_view::nameColumn, node_view::separatorRole, true);
    return *this;
}

ViewNodeDataBuilder& ViewNodeDataBuilder::withText(const QString& value)
{
    if (value.isEmpty())
        m_data->removeData(node_view::nameColumn, Qt::DisplayRole);
    else
        m_data->setData(node_view::nameColumn, Qt::DisplayRole, value);
    return *this;
}

ViewNodeDataBuilder& ViewNodeDataBuilder::withCheckedState(Qt::CheckState value)
{
    m_data->setData(node_view::checkMarkColumn, Qt::CheckStateRole, value);
    return *this;
}

ViewNodeDataBuilder& ViewNodeDataBuilder::withCheckable(bool checkable, Qt::CheckState state)
{
    if (checkable)
        return withCheckedState(state);

    m_data->removeData(node_view::checkMarkColumn, Qt::CheckStateRole);
    return *this;
}

ViewNodeDataBuilder& ViewNodeDataBuilder::withIcon(const QIcon& value)
{
    if (value.isNull())
        m_data->removeData(node_view::nameColumn, Qt::DecorationRole);
    else
        m_data->setData(node_view::nameColumn, Qt::DecorationRole, value);
    return *this;
}

ViewNodeDataBuilder& ViewNodeDataBuilder::withSiblingGroup(int value)
{
    m_data->setData(node_view::nameColumn, node_view::siblingGroupRole, value);
    return *this;
}

ViewNodeDataBuilder& ViewNodeDataBuilder::withCustomData(int column, int role, const QVariant& value)
{
    if (value.isNull())
        m_data->removeData(column, role);
    else
        m_data->setData(column, role, value);
    return *this;
}

const ViewNodeData& ViewNodeDataBuilder::data() const
{
    return *m_data;
}

} // namespace desktop
} // namespace client
} // namespace nx

