// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>

#include "resource_tree_globals.h"

namespace nx::vms::client::core {

using ResourceExtraStatusFlag = ResourceTree::ResourceExtraStatusFlag;

Q_DECLARE_FLAGS(ResourceExtraStatus, ResourceExtraStatusFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(ResourceExtraStatus)

NX_VMS_CLIENT_CORE_API ResourceExtraStatus getResourceExtraStatus(const QnResourcePtr& resource);

} // namespace nx::vms::client::core
