#include "analytics_metadata_provider_factory.h"

#include <nx/client/core/media/consuming_analytics_metadata_provider.h>
#include "local_file_analytics_metadata_provider.h"
#include "demo_analytics_metadata_provider.h"

namespace nx::vms::client::desktop {

void AnalyticsMetadataProviderFactory::registerMetadataProviders()
{
    registerMetadataFactory(
        "consuming",
        new core::ConsumingAnalyticsMetadataProviderFactory());
    registerMetadataFactory(
        "local",
        new LocalFileAnalyticsMetadataProviderFactory());
    registerMetadataFactory(
        "demo",
        new DemoAnalyticsMetadataProviderFactory());
}

} // namespace nx::vms::client::desktop
