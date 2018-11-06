#include "analytics_metadata_provider_factory.h"

#include <nx/client/core/media/consuming_analytics_metadata_provider.h>
#include "local_file_analytics_metadata_provider.h"
#include "demo_analytics_metadata_provider.h"

namespace nx::vms::client::desktop {

void AnalyticsMetadataProviderFactory::registerMetadataProviders()
{
    registerMetadataFactory(
        lit("consuming"),
        new core::ConsumingAnalyticsMetadataProviderFactory());
    registerMetadataFactory(
        lit("local"),
        new LocalFileAnalyticsMetadataProviderFactory());
    registerMetadataFactory(
        lit("demo"),
        new DemoAnalyticsMetadataProviderFactory());
}

} // namespace nx::vms::client::desktop
