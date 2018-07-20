#pragma once

#include <nx/utils/uuid.h>
#include <core/resource/resource_fwd.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node_fwd.h>
#include <nx/client/desktop/resource_views/node_view/node_view_constants.h>

namespace nx {
namespace client {
namespace desktop {
namespace helpers {

NodePtr createNode(
    const QString& caption,
    const NodeList& children,
    int siblingGroup = 0);

NodePtr createNode(
    const QString& caption,
    int siblingGroup = 0);

NodePtr createSeparatorNode(int siblingGroup = 0);
NodePtr createCheckAllNode(
    const QString& text,
    const QIcon& icon,
    int siblingGroup = 0);

NodePtr createAllLayoutsNode();

NodePtr createResourceNode(
    const QnResourcePtr& resource,
    bool checkable = false,
    Qt::CheckState nodeCheckedState = Qt::Unchecked);

using RelationCheckFunction =
    std::function<bool (const QnResourcePtr& parent, const QnResourcePtr& child)>;
using NodeCreationFunction = std::function<NodePtr (const QnResourcePtr& resource)>;

NodePtr createParentResourceNode(
    const QnResourcePtr& resource,
    const RelationCheckFunction& relationCheckFunction,
    const NodeCreationFunction& nodeCreationFunction = NodeCreationFunction(),
    bool checkable = false,
    Qt::CheckState nodeCheckedState = Qt::Unchecked,
    const QString& parentExtraTextTemplate = QString());

QnResourceList getLeafSelectedResources(const NodePtr& rootNode);

QnResourcePtr getResource(const NodePtr& node);

bool checkableNode(const NodePtr& node);
Qt::CheckState nodeCheckedState(const NodePtr& node);
QString nodeText(const NodePtr& node, int column = node_view::nameColumn);
QString nodeExtraText(const NodePtr& node, int column = node_view::nameColumn);

} // namespace helpers
} // namespace desktop
} // namespace client
} // namespace nx
