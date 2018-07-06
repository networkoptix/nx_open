#pragma once

#include <nx/client/core/media/analytics_metadata_provider_factory.h>
#include <nx/utils/singleton.h>

namespace nx {
namespace client {
namespace desktop {

class AnalyticsMetadataProviderFactory:
    public core::AnalyticsMetadataProviderFactory,
    public Singleton<AnalyticsMetadataProviderFactory>
{
public:
    void registerMetadataProviders();
};

} // namespace desktop
} // namespace client
} // namespace nx
