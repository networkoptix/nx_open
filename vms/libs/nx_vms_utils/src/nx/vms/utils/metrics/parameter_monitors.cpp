#include "parameter_providers.h"

namespace nx::vms::utils::metrics {

std::optional<Values> AbstractParameterMonitor::timeline(
    uint64_t /*nowSecsSinceEpoch*/, std::chrono::milliseconds /*length*/)
{
    return std::nullopt;
}

} // namespace nx::vms::utils::metrics
