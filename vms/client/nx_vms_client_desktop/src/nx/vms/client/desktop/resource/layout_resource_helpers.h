// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "resource_fwd.h"

namespace nx::vms::client::desktop {

NX_VMS_CLIENT_DESKTOP_API bool isPreviewSearchLayout(const core::LayoutResourcePtr& layout);
NX_VMS_CLIENT_DESKTOP_API bool isShowreelReviewLayout(const core::LayoutResourcePtr& layout);
NX_VMS_CLIENT_DESKTOP_API bool isVideoWallReviewLayout(const core::LayoutResourcePtr& layout);

} // namespace nx::vms::client::desktop
