#pragma once

#include <nx/client/core/media/abstract_analytics_metadata_provider.h>
#include <nx/client/core/media/analytics_metadata_provider_factory.h>

namespace nx {
namespace client {
namespace desktop {

class DemoAnalyticsMetadataProvider: public core::AbstractAnalyticsMetadataProvider
{
public:
    DemoAnalyticsMetadataProvider();

    virtual common::metadata::DetectionMetadataPacketPtr metadata(
        qint64 timestamp,
        int channel) override;

private:
    class Private;
    QScopedPointer<Private> const d;
};

class DemoAnalyticsMetadataProviderFactory:
    public core::AbstractAnalyticsMetadataProviderFactory
{
public:
    virtual bool supportsAnalytics(const QnResourcePtr& resource) const override;
    virtual core::AbstractAnalyticsMetadataProviderPtr createMetadataProvider(
        const QnResourcePtr& resource) const override;
};

} // namespace desktop
} // namespace client
} // namespace nx
