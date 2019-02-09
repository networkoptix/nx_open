#include "resource_thumbnail_provider.h"

#include <chrono>

#include <QtGui/QPainter>

#include <api/server_rest_connection.h>

#include <nx/vms/client/desktop/image_providers/camera_thumbnail_provider.h>
#include <nx/vms/client/desktop/image_providers/ffmpeg_image_provider.h>

#include <common/common_module.h>

#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/avi/avi_resource.h>

#include <nx/fusion/model_functions.h>

#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/style/nx_style.h>

namespace nx::vms::client::desktop {

struct ResourceThumbnailProvider::Private
{
    bool updateRequest(ResourceThumbnailProvider* q, const nx::api::ResourceImageRequest& value)
    {
        // TODO: #vkutin #gdm Recreate base provider only if its type has changed.

        baseProvider.reset();
        request = value;

        if (!NX_ASSERT(value.resource))
            return false;

        const auto mediaResource = value.resource.dynamicCast<QnMediaResource>();
        const bool useCustomSoundIcon = mediaResource && !mediaResource->hasVideo();

        QString placeholderIconPath;

        if (value.resource->hasFlags(Qn::server))
        {
            placeholderIconPath = lit("item_placeholders/videowall_server_placeholder.png");
        }
        else if (value.resource->hasFlags(Qn::web_page))
        {
            placeholderIconPath = lit("item_placeholders/videowall_webpage_placeholder.png");
        }
        else if (useCustomSoundIcon)
        {
            // Some cameras are actually provide only sound stream. So we draw sound icon for this.
            placeholderIconPath = lit("item_placeholders/sound.png");
        }
        else if (const auto camera = value.resource.dynamicCast<QnVirtualCameraResource>())
        {
            nx::api::CameraImageRequest cameraRequest(camera, request);
            cameraRequest.streamSelectionMode = streamSelectionMode;
            baseProvider.reset(new CameraThumbnailProvider(cameraRequest));
        }
        else if (const auto aviResource = value.resource.dynamicCast<QnAviResource>())
        {
            baseProvider.reset(new FfmpegImageProvider(value.resource,
                std::chrono::microseconds(request.usecSinceEpoch), request.size));
        }
        else
        {
            NX_ASSERT(false);
            return false;
        }

        if (!baseProvider && !placeholderIconPath.isEmpty())
        {
            QPixmap pixmap = qnSkin->pixmap(placeholderIconPath, true);
            // TODO: vms 4.0 has a new way to get preset colors
            const auto& palette = QnNxStyle::instance()->genericPalette();
            const auto& backgroundColor = palette.color(lit("dark"), 3);
            const auto& frameColor = palette.color(lit("dark"), 6);
            QSize size = pixmap.size();
            QPixmap dst(size);
            // We fill in the background.
            dst.fill(backgroundColor);
            QPainter painter(&dst);
            painter.setRenderHints(QPainter::SmoothPixmapTransform);
            painter.setOpacity(0.7);
            painter.drawPixmap(0, 0, pixmap);
            painter.setOpacity(1.0);

            baseProvider.reset(new BasicImageProvider(dst.toImage()));
        }

        if (baseProvider)
        {
            QObject::connect(baseProvider.data(), &ImageProvider::imageChanged,
                q, &ImageProvider::imageChanged);
            QObject::connect(baseProvider.data(), &ImageProvider::statusChanged,
                q, &ImageProvider::statusChanged);
            QObject::connect(baseProvider.data(), &ImageProvider::sizeHintChanged,
                q, &ImageProvider::sizeHintChanged);
        }

        return true;
    }

    QScopedPointer<ImageProvider> baseProvider;
    nx::api::ResourceImageRequest request;
    nx::api::CameraImageRequest::StreamSelectionMode streamSelectionMode;
};

ResourceThumbnailProvider::ResourceThumbnailProvider(
    const nx::api::ResourceImageRequest& request,
    QObject* parent)
    :
    base_type(parent),
    d(new Private())
{
    setRequestData(request);
}

ResourceThumbnailProvider::~ResourceThumbnailProvider()
{
}

nx::api::ResourceImageRequest ResourceThumbnailProvider::requestData() const
{
    return d->request;
}

void ResourceThumbnailProvider::setRequestData(const nx::api::ResourceImageRequest& request)
{
    const bool wasValid = status() != Qn::ThumbnailStatus::Invalid;
    d->updateRequest(this, request);

    if (wasValid)
    {
        emit imageChanged(image());
        emit sizeHintChanged(sizeHint());
        emit statusChanged(status());
    }
}

nx::api::CameraImageRequest::StreamSelectionMode
    ResourceThumbnailProvider::streamSelectionMode() const
{
    return d->streamSelectionMode;
}

void ResourceThumbnailProvider::setStreamSelectionMode(
    nx::api::CameraImageRequest::StreamSelectionMode value)
{
    if (d->streamSelectionMode == value)
        return;

    d->streamSelectionMode = value;

    if (d->request.resource.dynamicCast<QnVirtualCameraResource>())
        d->updateRequest(this, d->request);
}

QImage ResourceThumbnailProvider::image() const
{
    return d->baseProvider ? d->baseProvider->image() : QImage();
}

QSize ResourceThumbnailProvider::sizeHint() const
{
    return d->baseProvider ? d->baseProvider->sizeHint() : QSize();
}

Qn::ThumbnailStatus ResourceThumbnailProvider::status() const
{
    return d->baseProvider ? d->baseProvider->status() : Qn::ThumbnailStatus::Invalid;
}

void ResourceThumbnailProvider::doLoadAsync()
{
    if (d->baseProvider)
        d->baseProvider->loadAsync();
}

} // namespace nx::vms::client::desktop
