#include "server_resource.h"

#include <common/common_module.h>
#include <nx/metrics/metrics_storage.h>

namespace nx::vms::server {

qint64 MediaServerResource::getDelta(MediaServerResource::Metrics key, qint64 value)
{
    auto& counter = m_counters[key];
    qint64 result = value - counter;
    counter = value;
    return result;
}

qint64 MediaServerResource::getAndResetMetric(MediaServerResource::Metrics parameter)
{
    switch (parameter)
    {
    case Metrics::transactions:
    {
        const auto counter = commonModule()->metrics()->transactions().success() +
            commonModule()->metrics()->transactions().errors();
        return getDelta(parameter, counter);
    }
    default:
        return 0;
    }
}

} // namespace nx::vms::server
