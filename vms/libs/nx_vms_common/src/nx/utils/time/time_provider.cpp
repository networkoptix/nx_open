// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "time_provider.h"

#include <utils/common/synctime.h>

namespace nx::utils::time {

std::chrono::microseconds TimeProvider::currentTime() const
{
    return std::chrono::microseconds(qnSyncTime->currentUSecsSinceEpoch());
}

} // namespace nx::utils::time
