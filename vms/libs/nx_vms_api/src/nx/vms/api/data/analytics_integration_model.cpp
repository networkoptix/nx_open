// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_integration_model.h"

#include <nx/fusion/model_functions.h>
#include <nx/vms/api/analytics/plugin_manifest.h>

namespace nx::vms::api::analytics {

const QString kPluginManifestProperty("pluginManifest");
const QString kIntegrationTypeProperty("integrationType");

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    IntegrationModel, (json), nx_vms_api_analytics_IntegrationModel_Fields);

IntegrationModel::IntegrationModel(AnalyticsPluginData data):
    id(data.id),
    name(data.name),
    type(IntegrationType::sdk)
{
}

IntegrationModel::DbUpdateTypes IntegrationModel::toDbTypes() &&
{
    AnalyticsPluginData result;
    result.id = id;
    result.name = name;

    return result;
}

std::vector<IntegrationModel> IntegrationModel::fromDbTypes(DbListTypes data)
{
    std::map<nx::Uuid, analytics::PluginManifest> manifests;
    std::map<nx::Uuid, IntegrationType> integrationTypes;
    for (const auto& property: std::get<1>(data))
    {
        if (property.name == kPluginManifestProperty)
        {
            manifests[property.resourceId] =
                QJson::deserialized(property.value.toUtf8(), analytics::PluginManifest());
        }
        else if (property.name == kIntegrationTypeProperty)
        {
            integrationTypes[property.resourceId] =
                nx::reflect::fromString<IntegrationType>(property.value.toStdString());
        }
    }

    std::vector<IntegrationModel> result;
    for (auto& analyticsIntegrationData: std::get<0>(data))
    {
        IntegrationModel model(std::move(analyticsIntegrationData));
        if (manifests.contains(model.id))
        {
            auto& manifest = manifests[model.id];
            model.description = std::move(manifest.description);
            model.vendor = std::move(manifest.vendor);
            model.version = std::move(manifest.version);
            model.integrationId = std::move(manifest.id);
        }

        if (integrationTypes.contains(model.id))
            model.type = integrationTypes[model.id];

        result.push_back(std::move(model));
    }

    return result;
}

} // namespace nx::vms::api::analytics
