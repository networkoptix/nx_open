// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "abstract_loader_delegate.h"

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::mobile {
namespace timeline {

class BookmarkLoaderDelegate: public AbstractLoaderDelegate
{
    Q_OBJECT

public:
    explicit BookmarkLoaderDelegate(
        const QnResourcePtr& resource, int maxBookmarksPerBucket, QObject* parent = nullptr);
    virtual ~BookmarkLoaderDelegate() override;

    virtual QFuture<MultiObjectData> load(const QnTimePeriod& period,
        std::chrono::milliseconds minimumStackDuration) const override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace timeline
} // namespace nx::vms::client::mobile
