// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/analytics/analytics_attribute_helper.h>

#include "abstract_loader_delegate.h"

namespace nx::vms::client::core { class ChunkProvider; }

namespace nx::vms::client::mobile {
namespace timeline {

class AnalyticsLoaderDelegate: public AbstractLoaderDelegate
{
    Q_OBJECT

public:
    explicit AnalyticsLoaderDelegate(
        core::ChunkProvider* chunkProvider, int maxTracksPerBucket, QObject* parent = nullptr);
    virtual ~AnalyticsLoaderDelegate() override;

    virtual QFuture<MultiObjectData> load(const QnTimePeriod& period,
        std::chrono::milliseconds minimumStackDuration) const override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace timeline
} // namespace nx::vms::client::mobile
