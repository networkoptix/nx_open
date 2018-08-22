#pragma once

#include "../details/node/view_node_fwd.h"

#include <core/resource/resource_fwd.h>

namespace nx {
namespace client {
namespace desktop {
namespace node_view {

namespace details { class ViewNodeData; }

details::NodePtr createResourceNode(
    const QnResourcePtr& resource,
    const QString& extraText,
    bool checkable);

QnResourceList getSelectedResources(const details::NodePtr& root);
QnResourcePtr getResource(const details::NodePtr& node);
QnResourcePtr getResource(const QModelIndex& index);

QString extraText(const QModelIndex& node);
void setExtraText(const QString& value, const details::NodePtr& node, int column);
void setExtraText(const QString& value, const details::ViewNodeData& data, int column);

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx
