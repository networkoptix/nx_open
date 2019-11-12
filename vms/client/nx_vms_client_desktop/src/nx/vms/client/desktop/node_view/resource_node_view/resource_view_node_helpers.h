#pragma once

#include "../details/node/view_node_fwd.h"

#include <core/resource/resource_fwd.h>

namespace nx::vms::client::desktop {
namespace node_view {

namespace details { class ViewNodeData; }

bool isResourceColumn(const QModelIndex& index);

details::ViewNodeData getResourceNodeData(
    const QnResourcePtr& resource,
    int resourceColumn,
    const QString& extraText);

details::NodePtr createResourceNode(
    const QnResourcePtr& resource,
    const QString& extraText,
    bool checkable);

details::NodePtr createGroupNode(
    const QnVirtualCameraResourcePtr& cameraResource,
    const QString& extraText,
    bool checkable);

QnResourceList getSelectedResources(const details::NodePtr& root);
QnResourcePtr getResource(const details::NodePtr& node);
QnResourcePtr getResource(const QModelIndex& index);

QString extraText(const QModelIndex& index);
void setExtraText(const QString& value, const details::NodePtr& node, int column);

bool isValidResourceNode(const details::NodePtr& node);
bool isValidResourceNode(const QModelIndex& index);
details::ViewNodeData getDataForInvalidNode(bool invalid);
void setNodeValidState(const details::NodePtr& node, bool valid);

} // namespace node_view
} // namespace nx::vms::client::desktop
