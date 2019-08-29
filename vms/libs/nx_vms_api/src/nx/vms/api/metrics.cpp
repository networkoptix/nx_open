#include "metrics.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::metrics {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ValueManifest)(ValueGroupManifest)
    (AlarmRule)(ValueRule)
    (ResourceValues)(Alarm),
    (json), _Fields, (optional, true))

void merge(SystemValues* destination, SystemValues* source)
{
    for (auto& [group, sourceResources]: *source)
    {
        auto& destinationResources = (*destination)[group];
        for (auto& [resourceId, values]: sourceResources)
            destinationResources[resourceId] = std::move(values);
    }
}

SystemValues merge(std::vector<SystemValues> valuesList)
{
    SystemValues result;
    for (auto values: valuesList)
        merge(&result, &values);
    return result;
}

} // namespace nx::vms::api::metrics


