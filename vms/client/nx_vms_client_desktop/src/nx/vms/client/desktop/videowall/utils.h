#pragma once

#include <core/resource/resource_fwd.h>

#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {

void setVideoWallAutorunEnabled(const QnUuid& videoWallUuid, bool value);

/**
 * Set of screen indices, covered by the given videowall item.
 */
QSet<int> screensCoveredByItem(const QnVideoWallItem& item, const QnVideoWallResourcePtr& videoWall);

} // namespace nx::vms::client::desktop
