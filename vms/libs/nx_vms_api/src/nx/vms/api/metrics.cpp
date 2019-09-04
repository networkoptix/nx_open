#include "metrics.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::metrics {

void PrintTo(const Value& v, ::std::ostream* s)
{
    *s << QJson::serialized(v).toStdString();
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ValueManifest)(ValueGroupManifest)(AlarmRule)(ValueRule)(Alarm),
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

void merge(Alarms* destination, Alarms* source)
{
    destination->insert(destination->end(), source->begin(), source->end());
}

} // namespace nx::vms::api::metrics


