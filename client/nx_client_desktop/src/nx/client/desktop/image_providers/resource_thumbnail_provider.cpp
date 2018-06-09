#include "resource_thumbnail_provider.h"

#include <QtGui/QPainter>

#include <api/server_rest_connection.h>

#include <nx/client/desktop/image_providers/camera_thumbnail_provider.h>
#include <nx/client/desktop/image_providers/ffmpeg_image_provider.h>

#include <common/common_module.h>

#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/avi/avi_resource.h>

#include <nx/fusion/model_functions.h>

#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/style/nx_style.h>

namespace nx {
namespace client {
namespace desktop {

struct ResourceThumbnailProvider::Private
{
    bool updateRequest(const api::ResourceImageRequest& value)
    {
        request = value;

        QnResourcePtr resource = value.resource;

        NX_ASSERT(resource);

        QnMediaResourcePtr mediaResource = value.resource.dynamicCast<QnMediaResource>();
        bool useCustomSoundIcon = mediaResource && !mediaResource->hasVideo();

        QString placeholderIconPath;

        if (resource->hasFlags(Qn::server))
        {
            placeholderIconPath = lit("item_placeholders/videowall_server_placeholder.png");
        }
        else if (resource->hasFlags(Qn::web_page))
        {
            placeholderIconPath = lit("item_placeholders/videowall_webpage_placeholder.png");
        }
        else if(useCustomSoundIcon)
        {
            // Some cameras are actually provide only sound stream. So we draw sound icon for this.
            placeholderIconPath = lit("item_placeholders/sound.png");
        }
        else if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
        {
            api::CameraImageRequest cameraRequest(camera, request);
            baseProvider.reset(new CameraThumbnailProvider(cameraRequest));
        }
        else if (const auto aviResource = resource.dynamicCast<QnAviResource>())
        {
            baseProvider.reset(new FfmpegImageProvider(resource));
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

        return true;
    }

    QScopedPointer<ImageProvider> baseProvider;
    api::ResourceImageRequest request;
};

ResourceThumbnailProvider::ResourceThumbnailProvider(
    const api::ResourceImageRequest& request,
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

api::ResourceImageRequest ResourceThumbnailProvider::requestData() const
{
    return d->request;
}

void ResourceThumbnailProvider::setRequestData(const api::ResourceImageRequest& request)
{
    if (d->baseProvider)
    {
        d->baseProvider->disconnect(this);
        d->baseProvider.reset();
    }

    if (!d->updateRequest(request))
    {
        d->baseProvider.reset();
    }
    else if (auto p = d->baseProvider.data())
    {
        connect(p, &ImageProvider::imageChanged, this, &ImageProvider::imageChanged);
        connect(p, &ImageProvider::statusChanged, this, &ImageProvider::statusChanged);
        connect(p, &ImageProvider::sizeHintChanged, this, &ImageProvider::sizeHintChanged);
    }
}

QImage ResourceThumbnailProvider::image() const
 {
    if (d->baseProvider)
        return d->baseProvider->image();
    return QImage();
}

QSize ResourceThumbnailProvider::sizeHint() const
{
    if (d->baseProvider)
        return d->baseProvider->sizeHint();
    return QSize();
}

Qn::ThumbnailStatus ResourceThumbnailProvider::status() const
{
    if (d->baseProvider)
        return d->baseProvider->status();
    return Qn::ThumbnailStatus::Invalid;
}

void ResourceThumbnailProvider::doLoadAsync()
{
    if (d->baseProvider)
        d->baseProvider->loadAsync();
    else
    {
        emit statusChanged(Qn::ThumbnailStatus::Invalid);
    }
}

} // namespace desktop
} // namespace client
} // namespace nx
