// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_metadata_provider_factory.h"

#include <nx/vms/client/core/media/consuming_analytics_metadata_provider.h>

template<>
nx::vms::client::core::analytics::AnalyticsMetadataProviderFactory*
    Singleton<nx::vms::client::core::analytics::AnalyticsMetadataProviderFactory>::s_instance = nullptr;

namespace nx::vms::client::core::analytics {

void AnalyticsMetadataProviderFactory::registerMetadataProviders()
{
    registerMetadataFactory(
        "consuming",
        new core::ConsumingAnalyticsMetadataProviderFactory());
}

} // namespace nx::vms::client::core::analytics
