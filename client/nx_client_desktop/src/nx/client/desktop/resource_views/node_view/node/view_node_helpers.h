#pragma once

#include <nx/utils/uuid.h>
#include <core/resource/resource_fwd.h>
#include <nx/client/desktop/resource_views/node_view/node/view_node_fwd.h>
#include <nx/client/desktop/resource_views/node_view/node_view_constants.h>

namespace nx {
namespace client {
namespace desktop {

class ViewNodeData;

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

NodePtr nodeFromIndex(const QModelIndex& index);

QnResourceList getLeafSelectedResources(const NodePtr& rootNode);

bool isAllSiblingsCheckNode(const NodePtr& node);

//--
bool hasExpandedData(const ViewNodeData& data);
bool hasExpandedData(const QModelIndex& index);
bool expanded(const ViewNodeData& data);
bool expanded(const QModelIndex& index);

bool isSeparator(const QModelIndex& index);

int siblingGroup(const QModelIndex& index);

QString text(const QModelIndex& index);

QString extraText(const QModelIndex& index);

bool isCheckable(const NodePtr& node);
bool isCheckable(const QModelIndex& index);
Qt::CheckState checkedState(const NodePtr& node);
Qt::CheckState checkedState(const QModelIndex& index);

QnResourcePtr getResource(const NodePtr& node);
QnResourcePtr getResource(const QModelIndex& index);

} // namespace helpers
} // namespace desktop
} // namespace client
} // namespace nx
