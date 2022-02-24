// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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

details::ViewNodeData getGroupNodeData(
    const QnVirtualCameraResourcePtr& cameraResource,
    int resourceColumn,
    const QString& extraText,
    int checkColumn = -1);

QString extraText(const QModelIndex& index);

} // namespace node_view
} // namespace nx::vms::client::desktop
