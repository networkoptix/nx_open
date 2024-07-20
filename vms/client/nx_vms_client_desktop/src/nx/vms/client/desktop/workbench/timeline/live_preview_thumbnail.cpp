// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "live_preview_thumbnail.h"

#include <QtCore/QPointer>

#include <nx/utils/log/assert.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/core/thumbnails/async_image_result.h>
#include <nx/vms/client/desktop/workbench/timeline/thumbnail_loading_manager.h>

namespace nx::vms::client::desktop::workbench::timeline {

using namespace std::chrono;
using namespace nx::vms::client::core;

class LivePreviewRequest: public AsyncImageResult
{
    using base_type = AsyncImageResult;

public:
    LivePreviewRequest(ThumbnailLoadingManager* manager, milliseconds timestamp,
        QObject* parent = nullptr);

protected:
    virtual void setImage(const QImage& image) override;

private:
    ThumbnailLoadingManager* const m_manager;
    nx::utils::ScopedConnections m_managerConnections;
    milliseconds m_timestamp{};
};

struct LivePreviewThumbnail::Private
{
    QPointer<ThumbnailLoadingManager> manager;
    milliseconds timestamp{kNoTimestamp};
};

// -----------------------------------------------------------------------------------------------
// LivePreviewThumbnail

LivePreviewThumbnail::LivePreviewThumbnail(QObject* parent):
    base_type(parent),
    d(new Private{})
{
}

LivePreviewThumbnail::~LivePreviewThumbnail()
{
    // Required here for forward-declared scoped pointer destruction.
}

ThumbnailLoadingManager* LivePreviewThumbnail::thumbnailLoadingManager() const
{
    return d->manager;
}

void LivePreviewThumbnail::setThumbnailLoadingManager(ThumbnailLoadingManager* value)
{
    if (d->manager == value)
        return;

    d->manager = value;
    reset();

    if (d->manager && d->timestamp != kNoTimestamp)
        update();
}

milliseconds LivePreviewThumbnail::timestamp() const
{
    return d->timestamp;
}

void LivePreviewThumbnail::setTimestamp(milliseconds value)
{
    if (d->timestamp == value)
        return;

    d->timestamp = value;

    if (d->manager)
        update();
}

std::unique_ptr<AsyncImageResult> LivePreviewThumbnail::getImageAsync(
    bool /*forceRefresh*/) const
{
    if (!d->manager || d->timestamp == kNoTimestamp)
        return {};

    return std::make_unique<LivePreviewRequest>(d->manager, d->timestamp);
}

// -----------------------------------------------------------------------------------------------
// LivePreviewRequest

LivePreviewRequest::LivePreviewRequest(
    ThumbnailLoadingManager* manager,
    milliseconds timestamp,
    QObject* parent)
    :
    base_type(parent),
    m_manager(manager)
{
    if (!NX_ASSERT(m_manager))
    {
        setError("Invalid operation");
        return;
    }

    const auto thumbnail = m_manager->previewThumbnailAt(timestamp, &m_timestamp);
    if (thumbnail)
    {
        setImage(thumbnail->image());
        return;
    }

    m_managerConnections << connect(m_manager, &QObject::destroyed, this,
        [this]() { setError("Loading cancelled"); });

    m_managerConnections <<
        connect(m_manager, &ThumbnailLoadingManager::individualPreviewThumbnailLoaded, this,
            [this](ThumbnailPtr thumbnail)
            {
                if (thumbnail->time() == m_timestamp)
                    setImage(thumbnail->image());
            });
}

void LivePreviewRequest::setImage(const QImage& image)
{
    m_managerConnections.reset();
    AsyncImageResult::setImage(image);
}

} // namespace nx::vms::client::desktop::workbench::timeline
