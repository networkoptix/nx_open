// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/resource/layout_resource.h>

// nx::vms::client::desktop::LayoutResource is an alias for nx::vms::client::core::LayoutResource.
#include "resource_fwd.h"

namespace nx::vms::client::desktop {

NX_VMS_CLIENT_DESKTOP_API bool isPreviewSearchLayout(const LayoutResourcePtr& layout);
NX_VMS_CLIENT_DESKTOP_API bool isShowreelReviewLayout(const LayoutResourcePtr& layout);
NX_VMS_CLIENT_DESKTOP_API bool isVideoWallReviewLayout(const LayoutResourcePtr& layout);

} // namespace nx::vms::client::desktop
