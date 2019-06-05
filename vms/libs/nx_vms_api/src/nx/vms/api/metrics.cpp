#include "metrics.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::metrics {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ParameterRule)(ParameterGroupRules)
    (ParameterManifest)(ParameterGroupManifest)
    (ParameterValue)(ParameterGroupValues)(ResourceValues),
    (json), _Fields, (optional, true))

ParameterGroupManifest makeParameterManifest(QString id, QString name, QString unit)
{
    ParameterGroupManifest manifest;
    manifest.id = std::move(id);
    manifest.name = std::move(name);
    manifest.unit = std::move(unit);
    return manifest;
}

ParameterGroupManifest makeParameterGroupManifest(
    QString id, QString name, std::vector<ParameterGroupManifest> group)
{
    ParameterGroupManifest manifest = makeParameterManifest(std::move(id), std::move(name));
    manifest.group = std::move(group);
    return manifest;
}

ParameterGroupValues makeParameterValue(Value value, Status status)
{
    ParameterGroupValues parameter;
    parameter.value = std::move(value);
    parameter.status = std::move(status);
    return parameter;
}

ParameterGroupValues makeParameterGroupValue(std::map<QString, ParameterGroupValues> group)
{
    ParameterGroupValues parameter;
    parameter.group = std::move(group);
    return parameter;
}

void merge(Values* destination, Values* source)
{
    for (auto& [group, sourceResources]: *source)
    {
        auto& destinationResources = (*destination)[group];
        for (auto& [resourceId, values]: sourceResources)
            destinationResources[resourceId] = std::move(values);
    }
}

Values merge(std::vector<Values> valuesList)
{
    Values result;
    for (auto values: valuesList)
        merge(&result, &values);
    return result;
}

} // namespace nx::vms::api::metrics


