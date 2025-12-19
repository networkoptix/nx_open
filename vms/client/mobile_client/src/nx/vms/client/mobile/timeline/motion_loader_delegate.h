// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>

#include "abstract_loader_delegate.h"

namespace nx::vms::client::core { class ChunkProvider; }

namespace nx::vms::client::mobile {
namespace timeline {

class MotionLoaderDelegate: public AbstractLoaderDelegate
{
    Q_OBJECT

public:
    explicit MotionLoaderDelegate(
        core::ChunkProvider* chunkProvider, int maxMotionsPerBucket, QObject* parent = nullptr);
    virtual ~MotionLoaderDelegate() override;

    virtual QFuture<MultiObjectData> load(const QnTimePeriod& period,
        std::chrono::milliseconds minimumStackDuration) const override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace timeline
} // namespace nx::vms::client::mobile
