// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSharedPointer>

#include "analytics_metadata_provider_factory.h"
#include "abstract_metadata_consumer_owner.h"

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API ConsumingAnalyticsMetadataProvider:
    public AbstractAnalyticsMetadataProvider,
    public AbstractMetadataConsumerOwner
{
    using base_type = AbstractAnalyticsMetadataProvider;

public:
    ConsumingAnalyticsMetadataProvider();
    virtual ~ConsumingAnalyticsMetadataProvider() override;

    virtual nx::common::metadata::ObjectMetadataPacketPtr metadata(
        std::chrono::microseconds timestamp,
        int channel) const override;

    virtual QList<nx::common::metadata::ObjectMetadataPacketPtr> metadataRange(
        std::chrono::microseconds startTimestamp,
        std::chrono::microseconds endTimestamp,
        int channel,
        int maximumCount) const override;

    virtual QSharedPointer<media::AbstractMetadataConsumer> metadataConsumer() const override;

private:
    class Private;
    const QScopedPointer<Private> d;
};

class NX_VMS_CLIENT_CORE_API ConsumingAnalyticsMetadataProviderFactory:
    public AbstractAnalyticsMetadataProviderFactory
{
public:
    virtual bool supportsAnalytics(const QnResourcePtr& resource) const override;
    virtual AbstractAnalyticsMetadataProviderPtr createMetadataProvider(
        const QnResourcePtr& resource) const override;
};

} // namespace nx::vms::client::core
