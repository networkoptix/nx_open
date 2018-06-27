#include "parent_resource_node.h"

#include <client_core/client_core_module.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>

namespace nx {
namespace client {
namespace desktop {

NodePtr ParentResourceNode::create(
    const QnResourcePtr& resource,
    const RelationCheckFunction& relationCheckFunction,
    const NodeCreationFunction& nodeCreationFunction)
{
    auto raw = new ParentResourceNode(resource);
    const auto result = NodePtr(raw);
    const auto pool = qnClientCoreModule->commonModule()->resourcePool();

    const NodeCreationFunction creationFunction = nodeCreationFunction
        ? nodeCreationFunction
        : [](const QnResourcePtr& resource) -> NodePtr { return ResourceNode::create(resource); };

    for (const auto childResource: pool->getResources())
    {
        if (!relationCheckFunction(resource, childResource))
            continue;

        if (const auto node = creationFunction(childResource))
            raw->addNode(node);
    }
    return result;
}

ParentResourceNode::ParentResourceNode(const QnResourcePtr& resource):
    base_type(resource)
{
}

} // namespace desktop
} // namespace client
} // namespace nx

