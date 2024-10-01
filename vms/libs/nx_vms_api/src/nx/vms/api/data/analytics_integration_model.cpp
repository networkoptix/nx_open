// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_integration_model.h"

#include <nx/fusion/model_functions.h>
#include <nx/vms/api/analytics/integration_manifest.h>

namespace nx::vms::api::analytics {

// TODO: Rename the property to integrationManifest.
const QString kPluginManifestProperty("pluginManifest");

const QString kIntegrationTypeProperty("integrationType");

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    IntegrationModel, (json), nx_vms_api_analytics_IntegrationModel_Fields);

} // namespace nx::vms::api::analytics
