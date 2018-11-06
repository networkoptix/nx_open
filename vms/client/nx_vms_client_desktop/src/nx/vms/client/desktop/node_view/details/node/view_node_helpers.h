#pragma once

#include "view_node_fwd.h"

namespace nx::vms::client::desktop {
namespace node_view {
namespace details {

NX_VMS_DESKTOP_CLIENT_API NodePtr nodeFromIndex(const QModelIndex& index);

using ForEachNodeCallback = std::function<void (const NodePtr& node)>;
NX_VMS_DESKTOP_CLIENT_API void forEachLeaf(const NodePtr& root, const ForEachNodeCallback& callback);
NX_VMS_DESKTOP_CLIENT_API void forEachNode(const NodePtr& root, const ForEachNodeCallback& callback);

NX_VMS_DESKTOP_CLIENT_API bool expanded(const NodePtr& node);
NX_VMS_DESKTOP_CLIENT_API bool expanded(const ViewNodeData& data);
NX_VMS_DESKTOP_CLIENT_API bool expanded(const QModelIndex& index);

NX_VMS_DESKTOP_CLIENT_API bool isSeparator(const NodePtr& node);
NX_VMS_DESKTOP_CLIENT_API bool isSeparator(const ViewNodeData& data);
NX_VMS_DESKTOP_CLIENT_API bool isSeparator(const QModelIndex& index);

NX_VMS_DESKTOP_CLIENT_API int siblingGroup(const QModelIndex& index);

NX_VMS_DESKTOP_CLIENT_API QString text(const NodePtr& node, int column);
NX_VMS_DESKTOP_CLIENT_API QString text(const ViewNodeData& data, int column);
NX_VMS_DESKTOP_CLIENT_API QString text(const QModelIndex& index);

NX_VMS_DESKTOP_CLIENT_API bool checkable(const NodePtr& node, int column);
NX_VMS_DESKTOP_CLIENT_API bool checkable(const ViewNodeData& data, int column);
NX_VMS_DESKTOP_CLIENT_API bool checkable(const QModelIndex& index);

NX_VMS_DESKTOP_CLIENT_API Qt::CheckState checkedState(const NodePtr& node, int column);
NX_VMS_DESKTOP_CLIENT_API Qt::CheckState checkedState(const ViewNodeData& data, int column);
NX_VMS_DESKTOP_CLIENT_API Qt::CheckState checkedState(const QModelIndex& index);

NX_VMS_DESKTOP_CLIENT_API NodePtr createSimpleNode(
    const QString& caption,
    const NodeList& children,
    int checkableColumn = -1,
    int siblingGroup = 0);

NX_VMS_DESKTOP_CLIENT_API NodePtr createSimpleNode(
    const QString& caption,
    int checkableColumn = -1,
    int siblingGroup = 0);

NX_VMS_DESKTOP_CLIENT_API NodePtr createSeparatorNode(int siblingGroup = 0);

} // namespace details
} // namespace node_view
} // namespace nx::vms::client::desktop
