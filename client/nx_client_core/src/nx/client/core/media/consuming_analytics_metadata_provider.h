#pragma once

#include <QtCore/QSharedPointer>

#include "analytics_metadata_provider_factory.h"
#include "abstract_metadata_consumer_owner.h"

namespace nx {
namespace client {
namespace core {

class ConsumingAnalyticsMetadataProvider:
    public AbstractAnalyticsMetadataProvider,
    public AbstractMetadataConsumerOwner
{
    using base_type = AbstractAnalyticsMetadataProvider;

public:
    ConsumingAnalyticsMetadataProvider();

    virtual common::metadata::DetectionMetadataPacketPtr metadata(
        qint64 timestamp,
        int channel) const override;

    virtual QList<common::metadata::DetectionMetadataPacketPtr> metadataRange(
        qint64 startTimestamp,
        qint64 endTimestamp,
        int channel,
        int maximumCount = -1) const override;

    virtual QSharedPointer<media::AbstractMetadataConsumer> metadataConsumer() const override;

private:
    class Private;
    QScopedPointer<Private> const d;
};

class ConsumingAnalyticsMetadataProviderFactory: public AbstractAnalyticsMetadataProviderFactory
{
public:
    virtual bool supportsAnalytics(const QnResourcePtr& resource) const override;
    virtual AbstractAnalyticsMetadataProviderPtr createMetadataProvider(
        const QnResourcePtr& resource) const override;
};

} // namespace core
} // namespace client
} // namespace nx
