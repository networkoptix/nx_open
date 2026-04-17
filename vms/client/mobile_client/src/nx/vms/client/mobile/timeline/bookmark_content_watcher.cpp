// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_content_watcher.h"

#include <memory>

#include <QtCore/QTimer>

#include <camera/camera_bookmarks_manager.h>
#include <core/resource/resource.h>
#include <nx/vms/client/core/media/chunk_provider.h>
#include <nx/vms/client/core/system_context.h>

namespace nx::vms::client::mobile {
namespace timeline {

struct BookmarkContentWatcher::Private
{
    BookmarkContentWatcher* const q;
    const QPointer<core::ChunkProvider> resourceHolder;
    const std::unique_ptr<QTimer> updateTimer{new QTimer{}};
    rest::Handle requestHandle{};

    void updateState()
    {
        q->setHasContent(std::nullopt);
        updateTimer->stop();
        requestHandle = {};

        if (!NX_ASSERT(resourceHolder) || !resourceHolder->resource())
            return;

        requestUpdate();
        updateTimer->start();
    }

    void requestUpdate()
    {
        if (!NX_ASSERT(resourceHolder) || !resourceHolder->resource() || q->isForced())
            return;

        const auto systemContext =
            resourceHolder->resource()->systemContext()->as<core::SystemContext>();

        if (!systemContext || !NX_ASSERT(systemContext->connection()))
            return;

        QnCameraBookmarkSearchFilter filter;
        filter.cameras.insert(resourceHolder->resource()->getId());
        filter.limit = 1;

        requestHandle = systemContext->cameraBookmarksManager()->getBookmarksAsync(filter,
            [this](bool success, rest::Handle handle, QnCameraBookmarkList result)
            {
                if (handle == requestHandle && success)
                    q->setHasContent(!result.empty());
            },
            q);
    }
};

BookmarkContentWatcher::BookmarkContentWatcher(
    core::ChunkProvider* resourceHolder,
    std::chrono::seconds updateInterval)
    :
    d(new Private{.q = this, .resourceHolder = resourceHolder})
{
    if (!NX_ASSERT(d->resourceHolder))
        return;

    d->updateTimer->callOnTimeout([this]() { d->requestUpdate(); });
    d->updateTimer->setInterval(updateInterval);

    connect(d->resourceHolder, &core::ChunkProvider::resourceChanged, this,
        [this]() { d->updateState(); });

    d->updateState();
}

BookmarkContentWatcher::~BookmarkContentWatcher()
{
    // Required here for forward-declared scoped pointer destruction.
}

} // namespace timeline
} // namespace nx::vms::client::mobile
