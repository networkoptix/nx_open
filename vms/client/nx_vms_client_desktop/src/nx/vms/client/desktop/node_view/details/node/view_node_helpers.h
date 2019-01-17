#pragma once

#include "view_node_fwd.h"

namespace nx::vms::client::desktop {
namespace node_view {
namespace details {

NX_VMS_CLIENT_DESKTOP_API NodePtr nodeFromIndex(const QModelIndex& index);

using ForEachNodeCallback = std::function<void (const NodePtr& node)>;
NX_VMS_CLIENT_DESKTOP_API void forEachLeaf(const NodePtr& root, const ForEachNodeCallback& callback);
NX_VMS_CLIENT_DESKTOP_API void forEachNode(const NodePtr& root, const ForEachNodeCallback& callback);

NX_VMS_CLIENT_DESKTOP_API bool expanded(const NodePtr& node);
NX_VMS_CLIENT_DESKTOP_API bool expanded(const ViewNodeData& data);
NX_VMS_CLIENT_DESKTOP_API bool expanded(const QModelIndex& index);

NX_VMS_CLIENT_DESKTOP_API bool isSeparator(const NodePtr& node);
NX_VMS_CLIENT_DESKTOP_API bool isSeparator(const ViewNodeData& data);
NX_VMS_CLIENT_DESKTOP_API bool isSeparator(const QModelIndex& index);

/**
* Assuming that the sort order is ascending, nodes with lesser group sort order property will go
*     before nodes with greater group sort order property regardless any other contents.
*     Group sort order may be either positive or negative, default value is 0.
*
* @param index Model index representing node stored in NodeViewModel.
*/
NX_VMS_CLIENT_DESKTOP_API int groupSortOrder(const QModelIndex& index);

NX_VMS_CLIENT_DESKTOP_API QString text(const NodePtr& node, int column);
NX_VMS_CLIENT_DESKTOP_API QString text(const ViewNodeData& data, int column);
NX_VMS_CLIENT_DESKTOP_API QString text(const QModelIndex& index);

NX_VMS_CLIENT_DESKTOP_API bool checkable(const NodePtr& node, int column);
NX_VMS_CLIENT_DESKTOP_API bool checkable(const ViewNodeData& data, int column);
NX_VMS_CLIENT_DESKTOP_API bool checkable(const QModelIndex& index);

NX_VMS_CLIENT_DESKTOP_API Qt::CheckState checkedState(const NodePtr& node, int column);
NX_VMS_CLIENT_DESKTOP_API Qt::CheckState checkedState(const ViewNodeData& data, int column);
NX_VMS_CLIENT_DESKTOP_API Qt::CheckState checkedState(const QModelIndex& index);

NX_VMS_CLIENT_DESKTOP_API NodePtr createSimpleNode(
    const QString& caption,
    const NodeList& children,
    int checkableColumn = -1,
    int groupSortOrder = 0);

NX_VMS_CLIENT_DESKTOP_API NodePtr createSimpleNode(
    const QString& caption,
    int checkableColumn = -1,
    int groupSortOrder = 0);

NX_VMS_CLIENT_DESKTOP_API NodePtr createSeparatorNode(int groupSortOrder = 0);

} // namespace details
} // namespace node_view
} // namespace nx::vms::client::desktop
