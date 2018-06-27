#include "resource_node.h"

#include <core/resource/resource.h>

namespace nx {
namespace client {
namespace desktop {

NodePtr ResourceNode::create(const QnResourcePtr& resource)
{
    return NodePtr(new ResourceNode(resource));
}

ResourceNode::ResourceNode(const QnResourcePtr& resource):
    m_resource(resource)
{
}

QVariant ResourceNode::data(int column, int role) const
{
    switch(role)
    {
        case Qt::DisplayRole:
            // TODO: add constants for columns
            return column == 0 && m_resource ? m_resource->getName() : QString();
        default:
            return base_type::data(column, role);
    }
}


} // namespace desktop
} // namespace client
} // namespace nx

