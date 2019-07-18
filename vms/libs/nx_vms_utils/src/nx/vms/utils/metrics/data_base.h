#pragma once

#include <nx/vms/api/metrics.h>
#include <nx/utils/value_history.h>

namespace nx::vms::utils::metrics {

using Value
    = api::metrics::Value;

// TODO: Should be replaced with some storage based data base so it survives restarts.
using DataBase
    = nx::utils::TreeValueHistory<QString, Value>;

} // namespace nx::vms::utils::metrics
