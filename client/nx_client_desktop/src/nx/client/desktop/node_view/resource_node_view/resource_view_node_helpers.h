#pragma once

#include "../details/node/view_node_fwd.h"

#include <core/resource/resource_fwd.h>

namespace nx {
namespace client {
namespace desktop {
namespace node_view {

details::NodePtr createResourceNode(
    const QnResourcePtr& resource,
    const details::OptionalCheckedState& checkedState = details::OptionalCheckedState());

using RelationCheckFunction =
    std::function<bool (const QnResourcePtr& parent, const QnResourcePtr& child)>;
using NodeCreationFunction = std::function<details::NodePtr (const QnResourcePtr& resource)>;
using ChildrenCountExtraTextGenerator = std::function<QString (int count)>;
details::NodePtr createParentResourceNode(
    const QnResourcePtr& resource,
    const RelationCheckFunction& relationCheckFunction,
    const NodeCreationFunction& nodeCreationFunction = NodeCreationFunction(),
    const details::OptionalCheckedState& checkedState = details::OptionalCheckedState(),
    const ChildrenCountExtraTextGenerator& extraTextGenerator = ChildrenCountExtraTextGenerator());

QnResourceList getSelectedResources(const details::NodePtr& root);
QnResourcePtr getResource(const details::NodePtr& node);
QnResourcePtr getResource(const QModelIndex& index);

QString extraText(const QModelIndex& node);

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx
