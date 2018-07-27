#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/client/desktop/resource_views/node_view/node/view_node_fwd.h>

namespace nx {
namespace client {
namespace desktop {
namespace node_view {
namespace helpers {

NodePtr createResourceNode(
    const QnResourcePtr& resource,
    const OptionalCheckedState& checkedState = OptionalCheckedState());

using RelationCheckFunction =
    std::function<bool (const QnResourcePtr& parent, const QnResourcePtr& child)>;
using NodeCreationFunction = std::function<NodePtr (const QnResourcePtr& resource)>;
using ChildrenCountExtraTextGenerator = std::function<QString (int count)>;

NodePtr createParentResourceNode(
    const QnResourcePtr& resource,
    const RelationCheckFunction& relationCheckFunction,
    const NodeCreationFunction& nodeCreationFunction = NodeCreationFunction(),
    const OptionalCheckedState& checkedState = OptionalCheckedState(),
    const ChildrenCountExtraTextGenerator& extraTextGenerator = ChildrenCountExtraTextGenerator());

QnResourceList getSelectedResources(const NodePtr& root);

QnResourcePtr getResource(const NodePtr& node);
QnResourcePtr getResource(const QModelIndex& index);

} // namespace helpers
} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx
