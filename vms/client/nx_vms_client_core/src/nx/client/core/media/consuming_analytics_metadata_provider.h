#pragma once

#include <QtCore/QSharedPointer>

#include "analytics_metadata_provider_factory.h"
#include "abstract_metadata_consumer_owner.h"

namespace nx::vms::client::core {

class ConsumingAnalyticsMetadataProvider:
    public AbstractAnalyticsMetadataProvider,
    public AbstractMetadataConsumerOwner
{
    using base_type = AbstractAnalyticsMetadataProvider;

public:
    ConsumingAnalyticsMetadataProvider();
    virtual ~ConsumingAnalyticsMetadataProvider() override;

    virtual nx::common::metadata::DetectionMetadataPacketPtr metadata(
        std::chrono::microseconds timestamp,
        int channel) const override;

    virtual QList<nx::common::metadata::DetectionMetadataPacketPtr> metadataRange(
        std::chrono::microseconds startTimestamp,
        std::chrono::microseconds endTimestamp,
        int channel,
        int maximumCount) const override;

    virtual QSharedPointer<media::AbstractMetadataConsumer> metadataConsumer() const override;

private:
    class Private;
    const QScopedPointer<Private> d;
};

class ConsumingAnalyticsMetadataProviderFactory:
    public AbstractAnalyticsMetadataProviderFactory
{
public:
    virtual bool supportsAnalytics(const QnResourcePtr& resource) const override;
    virtual AbstractAnalyticsMetadataProviderPtr createMetadataProvider(
        const QnResourcePtr& resource) const override;
};

} // namespace nx::vms::client::core
