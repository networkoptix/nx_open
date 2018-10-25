#pragma once

#include <nx/client/core/media/analytics_metadata_provider_factory.h>
#include <nx/utils/singleton.h>

namespace nx::client::desktop {

class AnalyticsMetadataProviderFactory:
    public core::AnalyticsMetadataProviderFactory,
    public Singleton<AnalyticsMetadataProviderFactory>
{
public:
    void registerMetadataProviders();
};

} // namespace nx::client::desktop
