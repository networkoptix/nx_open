// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "chunk_content_watcher.h"

#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/media/chunk_provider.h>

namespace nx::vms::client::mobile {
namespace timeline {

struct ChunkContentWatcher::Private
{
    ChunkContentWatcher* const q;
    const QPointer<core::ChunkProvider> chunkProvider;
    const Qn::TimePeriodContent contentType;

    void update()
    {
        if (!NX_ASSERT(chunkProvider)
            || !chunkProvider->resource()
            || chunkProvider->isLoadingContent(contentType))
        {
            q->setHasContent(std::nullopt);
        }
        else
        {
            q->setHasContent(chunkProvider->hasPeriods(contentType));
        }
    }
};

ChunkContentWatcher::ChunkContentWatcher(
    core::ChunkProvider* chunkProvider,
    Qn::TimePeriodContent contentType)
    :
    d(new Private{.q = this, .chunkProvider = chunkProvider, .contentType = contentType})
{
    if (!NX_ASSERT(d->chunkProvider))
        return;

    connect(d->chunkProvider, &core::ChunkProvider::periodsUpdated, this,
        [this](Qn::TimePeriodContent contentType)
        {
            if (contentType == d->contentType)
                d->update();
        });

    connect(d->chunkProvider, &core::ChunkProvider::loadingContentChanged, this,
        [this](Qn::TimePeriodContent contentType)
        {
            if (contentType == d->contentType)
                d->update();
        });

    d->update();
}

ChunkContentWatcher::~ChunkContentWatcher()
{
    // Required here for forward-declared scoped pointer destruction.
}

} // namespace timeline
} // namespace nx::vms::client::mobile
