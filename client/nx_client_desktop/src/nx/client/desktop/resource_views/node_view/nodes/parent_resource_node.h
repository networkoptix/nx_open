#pragma once

#include <nx/client/desktop/resource_views/node_view/nodes/resource_node.h>

namespace nx {
namespace client {
namespace desktop {

class ParentResourceNode: public ResourceNode
{
    using base_type = ResourceNode;

public:
    using RelationCheckFunction =
        std::function<bool (const QnResourcePtr& parent, const QnResourcePtr& child)>;
    using NodeCreationFunction =
        std::function<NodePtr (const QnResourcePtr& resource)>;

    static NodePtr create(
        const QnResourcePtr& resource,
        const RelationCheckFunction& relationCheckFunction,
        const NodeCreationFunction& nodeCreationFunction = NodeCreationFunction(),
        bool checkable = false);

private:
    ParentResourceNode(const QnResourcePtr& resource, bool checkable);
};

} // namespace desktop
} // namespace client
} // namespace nx
