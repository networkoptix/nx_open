#include "parameter_providers.h"

namespace nx::vms::utils::metrics {

std::optional<Values> AbstractParameterMonitor::timeline(
    std::chrono::milliseconds /*length*/) const
{
    return std::nullopt;
}

} // namespace nx::vms::utils::metrics
