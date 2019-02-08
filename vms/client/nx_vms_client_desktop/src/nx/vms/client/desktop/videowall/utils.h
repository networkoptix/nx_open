#pragma once

#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {

void setVideoWallAutorunEnabled(const QnUuid& videoWallUuid, bool value);

} // namespace nx::vms::client::desktop
