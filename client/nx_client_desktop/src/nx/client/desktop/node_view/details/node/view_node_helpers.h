#pragma once

#include "view_node_fwd.h"

namespace nx {
namespace client {
namespace desktop {
namespace node_view {
namespace details {

class ViewNodeData;

NodePtr nodeFromIndex(const QModelIndex& index);

bool expanded(const ViewNodeData& data);
bool expanded(const QModelIndex& index);

bool isSeparator(const QModelIndex& index);

int siblingGroup(const QModelIndex& index);

QString text(const QModelIndex& index);

bool isCheckable(const NodePtr& node, int column);
bool isCheckable(const QModelIndex& index);
bool isCheckable(const ViewNodeData& data, int column);

Qt::CheckState checkedState(const NodePtr& node, int column);
Qt::CheckState checkedState(const QModelIndex& index);

NodePtr createSimpleNode(
    const QString& caption,
    const NodeList& children,
    int siblingGroup = 0);

NodePtr createSimpleNode(
    const QString& caption,
    int siblingGroup = 0);

NodePtr createSeparatorNode(int siblingGroup = 0);

} // namespace details
} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx
