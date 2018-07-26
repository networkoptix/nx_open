#pragma once

#include <nx/utils/uuid.h>
#include <nx/client/desktop/resource_views/node_view/node/view_node_fwd.h>
#include <nx/client/desktop/resource_views/node_view/node_view_constants.h>

namespace nx {
namespace client {
namespace desktop {

class ViewNodeData;

namespace node_view {
namespace helpers {

NodePtr createSimpleNode(
    const QString& caption,
    const NodeList& children,
    int siblingGroup = 0);

NodePtr createSimpleNode(
    const QString& caption,
    int siblingGroup = 0);

NodePtr createSeparatorNode(int siblingGroup = 0);

NodePtr createCheckAllNode(
    const QString& text,
    const QIcon& icon,
    int siblingGroup = 0);

NodePtr nodeFromIndex(const QModelIndex& index);

bool isAllSiblingsCheckNode(const NodePtr& node);

//--
bool expanded(const ViewNodeData& data);
bool expanded(const QModelIndex& index);

bool isSeparator(const QModelIndex& index);

int siblingGroup(const QModelIndex& index);

QString text(const QModelIndex& index);

QString extraText(const QModelIndex& index);

//bool isCheckable(const NodePtr& node, int column);
//bool isCheckable(const QModelIndex& index);
bool isCheckable(const ViewNodeData& data, int column);

//Qt::CheckState checkedState(const NodePtr& node, int column);
//Qt::CheckState checkedState(const QModelIndex& index);

} // namespace helpers
} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx
