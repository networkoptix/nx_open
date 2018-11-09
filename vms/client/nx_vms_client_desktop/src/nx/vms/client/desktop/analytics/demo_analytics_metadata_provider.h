#pragma once

#include <nx/client/core/media/abstract_analytics_metadata_provider.h>
#include <nx/client/core/media/analytics_metadata_provider_factory.h>

namespace nx::vms::client::desktop {

class DemoAnalyticsMetadataProvider: public core::AbstractAnalyticsMetadataProvider
{
public:
    DemoAnalyticsMetadataProvider();

    virtual nx::common::metadata::DetectionMetadataPacketPtr metadata(
        std::chrono::microseconds timestamp,
        int channel) const override;

    virtual QList<nx::common::metadata::DetectionMetadataPacketPtr> metadataRange(
        std::chrono::microseconds startTimestamp,
        std::chrono::microseconds endTimestamp,
        int channel,
        int maximumCount) const override;

private:
    class Private;
    const QScopedPointer<Private> d;
};

class DemoAnalyticsMetadataProviderFactory:
    public core::AbstractAnalyticsMetadataProviderFactory
{
public:
    virtual bool supportsAnalytics(const QnResourcePtr& resource) const override;
    virtual core::AbstractAnalyticsMetadataProviderPtr createMetadataProvider(
        const QnResourcePtr& resource) const override;
};

} // namespace nx::vms::client::desktop
