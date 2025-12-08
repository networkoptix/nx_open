// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>

namespace nx::vms::client::core {
namespace entity_resource_tree {
namespace resource_grouping {

NX_VMS_CLIENT_CORE_API QString getRecorderCameraGroupId(const QnResourcePtr& resource, int order);
NX_VMS_CLIENT_CORE_API QString getUserDefinedGroupId(const QnResourcePtr& resource, int order);

} // namespace resource_grouping
} // namespace entity_resource_tree
} // namespace nx::vms::client::mobile
