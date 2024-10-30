// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "integration_manifest.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::analytics {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(IntegrationManifest, (json),
    nx_vms_api_analytics_IntegrationManifest_Fields, (brief, true))

std::vector<ManifestError> validate(const IntegrationManifest& integrationManifest)
{
    std::vector<ManifestError> result;
    if (integrationManifest.id.isEmpty())
        result.emplace_back(ManifestErrorType::emptyIntegrationId);

    if (integrationManifest.name.isEmpty())
        result.emplace_back(ManifestErrorType::emptyIntegrationName);

    if (integrationManifest.description.isEmpty())
        result.emplace_back(ManifestErrorType::emptyIntegrationDescription);

    // NOTE: `version` is optional.

    // NOTE: `vendor` is allowed to be omitted because some Nx plugins may have it empty.

    return result;
}

} // namespace nx::vms::api::analytics
