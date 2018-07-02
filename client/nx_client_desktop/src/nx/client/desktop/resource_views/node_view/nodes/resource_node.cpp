#include "resource_node.h"

#include <core/resource/resource.h>
#include <nx/client/desktop/resource_views/node_view/node_view_constants.h>
#include <ui/style/resource_icon_cache.h>

namespace nx {
namespace client {
namespace desktop {

NodePtr ResourceNode::create(const QnResourcePtr& resource, bool checkable)
{
    return NodePtr(new ResourceNode(resource, checkable));
}

ResourceNode::ResourceNode(const QnResourcePtr& resource, bool checkable):
    base_type(checkable),
    m_resource(resource)
{
}

QVariant ResourceNode::data(int column, int role) const
{
    switch(role)
    {
        case Qt::DisplayRole:
            return column == NodeViewColumn::Name && m_resource
                ? m_resource->getName()
                : base_type::data(column, role);
        case Qt::DecorationRole:
            return column == NodeViewColumn::Name && m_resource
                ? qnResIconCache->icon(m_resource)
                : base_type::data(column, role);
        case Qt::CheckStateRole:
            return column == NodeViewColumn::CheckMark && checkable() ? checked() : QVariant();

        default:
            return base_type::data(column, role);
    }
}

Qt::ItemFlags ResourceNode::flags(int column) const
{
    const auto baseFlags = base_type::flags(column);
    return checkable() && column == NodeViewColumn::CheckMark
        ? baseFlags | Qt::ItemIsUserCheckable
        : baseFlags;
}

} // namespace desktop
} // namespace client
} // namespace nx

