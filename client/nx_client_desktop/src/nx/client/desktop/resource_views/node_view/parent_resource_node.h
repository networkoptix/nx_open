#pragma once

#include <nx/client/desktop/resource_views/node_view/resource_node.h>

namespace nx {
namespace client {
namespace desktop {

class ParentResourceNode: public ResourceNode
{
    using base_type = ResourceNode;

public:
    using RelationCheckFunction =
        std::function<bool (const QnResourcePtr& parent, const QnResourcePtr& child)>;

    static NodePtr create(
        const QnResourcePtr& resource,
        const RelationCheckFunction& relationCheckFunction);

private:
    ParentResourceNode(const QnResourcePtr& resource);
};

} // namespace desktop
} // namespace client
} // namespace nx
