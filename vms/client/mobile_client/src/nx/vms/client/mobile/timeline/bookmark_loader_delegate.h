// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/camera_bookmark_fwd.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/timeline/backends/abstract_backend.h>

#include "abstract_loader_delegate.h"

namespace nx::vms::client::mobile {
namespace timeline {

class BookmarkLoaderDelegate: public AbstractLoaderDelegate
{
    Q_OBJECT
    using BackendPtr = core::timeline::BackendPtr<QnCameraBookmarkList>;

public:
    explicit BookmarkLoaderDelegate(const BackendPtr& backend, int maxBookmarksPerBucket,
        QObject* parent = nullptr);

    virtual ~BookmarkLoaderDelegate() override;

    virtual QFuture<MultiObjectData> load(const QnTimePeriod& period,
        std::chrono::milliseconds minimumStackDuration) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace timeline
} // namespace nx::vms::client::mobile
