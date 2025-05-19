// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_resource_helpers.h"

#include <client/client_globals.h>
#include <nx/vms/client/core/resource/layout_resource.h>

namespace nx::vms::client::desktop {

bool isPreviewSearchLayout(const core::LayoutResourcePtr& layout)
{
    return layout && layout->data().contains(Qn::LayoutSearchStateRole);
}

bool isShowreelReviewLayout(const core::LayoutResourcePtr& layout)
{
    return layout && layout->data().contains(Qn::ShowreelUuidRole);
}

bool isVideoWallReviewLayout(const core::LayoutResourcePtr& layout)
{
    return layout && layout->data().contains(Qn::VideoWallResourceRole);
}

} // namespace nx::vms::client::desktop
