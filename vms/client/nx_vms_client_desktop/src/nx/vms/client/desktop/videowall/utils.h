// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>

namespace nx::vms::client::desktop {

/**
 * Set of screen indices, covered by the given videowall item.
 */
QSet<int> screensCoveredByItem(
    const QnVideoWallItem& item,
    const QnVideoWallResourcePtr& videoWall);

} // namespace nx::vms::client::desktop
