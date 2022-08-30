// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_integration_model.h"

#include <nx/fusion/model_functions.h>
#include <nx/vms/api/analytics/plugin_manifest.h>

namespace nx::vms::api {

static const QString kPluginManifestProperty("pluginManifest");

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    AnalyticsIntegrationModel, (json), nx_vms_api_AnalyticsIntegrationModel_Fields);

AnalyticsIntegrationModel::AnalyticsIntegrationModel(AnalyticsPluginData data):
    id(data.id),
    name(data.name)
{
}

AnalyticsIntegrationModel::DbUpdateTypes AnalyticsIntegrationModel::toDbTypes() &&
{
    AnalyticsPluginData result;
    result.id = id;
    result.name = name;

    return result;
}

std::vector<AnalyticsIntegrationModel> AnalyticsIntegrationModel::fromDbTypes(DbListTypes data)
{
    std::map<QnUuid, analytics::PluginManifest> manifests;
    for (const auto& property: std::get<1>(data))
    {
        if (property.name == kPluginManifestProperty)
        {
            manifests[property.resourceId] =
                QJson::deserialized(property.value.toUtf8(), analytics::PluginManifest());
        }
    }

    std::vector<AnalyticsIntegrationModel> result;
    for (auto& analyticsIntegrationData: std::get<0>(data))
    {
        AnalyticsIntegrationModel model(std::move(analyticsIntegrationData));
        if (manifests.contains(model.id))
        {
            auto& manifest = manifests[model.id];
            model.description = std::move(manifest.description);
            model.vendor = std::move(manifest.vendor);
            model.version = std::move(manifest.version);
            model.integrationId = std::move(manifest.id);
        }
        result.push_back(std::move(model));
    }

    return result;
}

AnalyticsIntegrationModel fromDb(AnalyticsPluginData data)
{
    return AnalyticsIntegrationModel(std::move(data));
}

} // namespace nx::vms::api
