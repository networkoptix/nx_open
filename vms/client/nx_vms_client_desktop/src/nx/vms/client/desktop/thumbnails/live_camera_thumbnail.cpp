// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "live_camera_thumbnail.h"

#include <QtQml/QtQml>

#include <core/resource/camera_resource.h>

#include <nx/api/mediaserver/image_request.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/format.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/thumbnails/thumbnail_cache.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_context.h>

namespace nx::vms::client::desktop {

using namespace nx::vms::client::core;

//-------------------------------------------------------------------------------------------------
// LiveCameraThumbnail::Private

struct LiveCameraThumbnail::Private
{
    LiveCameraThumbnail* const q;

    CameraStream stream = CameraStream::undefined;

    static ThumbnailCache* cache()
    {
        static ThumbnailCache cache;
        return &cache;
    }
};

//-------------------------------------------------------------------------------------------------
// LiveCameraThumbnail

LiveCameraThumbnail::LiveCameraThumbnail(QObject* parent):
    base_type(Private::cache(), parent),
    d(new Private{this})
{
    setMaximumSize(160); //< Default maximum size.
}

LiveCameraThumbnail::~LiveCameraThumbnail()
{
    // Required here for forward-declared scoped pointer destruction.
}

LiveCameraThumbnail::CameraStream LiveCameraThumbnail::stream() const
{
    return d->stream;
}

void LiveCameraThumbnail::setStream(CameraStream value)
{
    if (d->stream == value)
        return;

    d->stream = value;
    reset();

    emit streamChanged();
    update();
}

ThumbnailCache* LiveCameraThumbnail::thumbnailCache()
{
    return Private::cache();
}

void LiveCameraThumbnail::registerQmlType()
{
    qmlRegisterType<LiveCameraThumbnail>("nx.vms.client.desktop", 1, 0,
        "LiveCameraThumbnail");
}

std::unique_ptr<AsyncImageResult> LiveCameraThumbnail::getImageAsyncUncached() const
{
    const auto camera = resource().objectCast<QnVirtualCameraResource>();
    if (!camera || !NX_ASSERT(active()))
        return {};

    return std::make_unique<CameraAsyncImageRequest>(camera, maximumSize(), d->stream);
}

QString LiveCameraThumbnail::thumbnailId() const
{
    return nx::format("%1:%2").args(base_type::thumbnailId(), d->stream);
}

} // namespace nx::vms::client::desktop
