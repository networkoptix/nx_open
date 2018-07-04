#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node_fwd.h>

namespace nx {
namespace client {
namespace desktop {
namespace helpers {

NodePtr createParentedLayoutsNode();
NodePtr createCurrentUserLayoutsNode();

NodePtr createResourceNode(
    const QnResourcePtr& resource,
    bool checkable = false,
    Qt::CheckState checkedState = Qt::Unchecked);

using RelationCheckFunction =
    std::function<bool (const QnResourcePtr& parent, const QnResourcePtr& child)>;
using NodeCreationFunction = std::function<NodePtr (const QnResourcePtr& resource)>;

NodePtr createParentResourceNode(
    const QnResourcePtr& resource,
    const RelationCheckFunction& relationCheckFunction,
    const NodeCreationFunction& nodeCreationFunction = NodeCreationFunction(),
    bool checkable = false,
    Qt::CheckState checkedState = Qt::Unchecked);

QnResourceList getLeafSelectedResources(const NodePtr& rootNode);

} // namespace helpers
} // namespace desktop
} // namespace client
} // namespace nx
