#include "parent_resource_node.h"

#include <client_core/client_core_module.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>

namespace nx {
namespace client {
namespace desktop {

NodePtr ParentResourceNode::create(
    const QnResourcePtr& resource,
    const RelationCheckFunction& relationCheckFunction)
{
    auto raw = new ParentResourceNode(resource);
    const auto result = NodePtr(raw);
    const auto pool = qnClientCoreModule->commonModule()->resourcePool();
    for (const auto childResource: pool->getResources())
    {
        if (relationCheckFunction(resource, childResource))
            raw->addNode(ResourceNode::create(childResource));
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

