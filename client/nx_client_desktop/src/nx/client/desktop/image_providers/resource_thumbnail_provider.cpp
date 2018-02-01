#include "resource_thumbnail_provider.h"

#include <QtGui/QPainter>

#include <api/server_rest_connection.h>

#include <nx/client/desktop/image_providers/camera_thumbnail_provider.h>
#include <nx/client/desktop/image_providers/ffmpeg_image_provider.h>

#include <common/common_module.h>

#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>

#include <nx/fusion/model_functions.h>

#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/style/nx_style.h>

namespace nx {
namespace client {
namespace desktop {

struct ResourceThumbnailProvider::Private
{
    void updateRequest(const api::ResourceImageRequest& value)
    {
        request = value;

        QnMediaResourcePtr mediaResource = request.resource.dynamicCast<QnMediaResource>();
        // Some cameras are actually provide only sound stream. So we draw sound icon for this.
        if (mediaResource && !mediaResource->hasVideo())
        {
            // TODO: vms 4.0 has a new way to get preset colors
            const auto& palette = QnNxStyle::instance()->genericPalette();
            const auto& backgroundColor = palette.color(lit("dark"), 4);
            const auto& frameColor = palette.color(lit("dark"), 6);
            QPixmap pixmap = qnSkin->pixmap(lit("item_placeholders/sound.png"), true);
            QSize size = pixmap.size();
            QPixmap dst(size);
            // We fill in background.
            dst.fill(backgroundColor);
            QPainter painter(&dst);
            painter.setRenderHints(QPainter::SmoothPixmapTransform);
            painter.setOpacity(0.7);
            painter.drawPixmap(0, 0, pixmap);
            painter.setOpacity(1.0);

            baseProvider.reset(new QnBasicImageProvider(dst.toImage()));
        }
        else if (const auto camera = request.resource.dynamicCast<QnVirtualCameraResource>())
        {
            api::CameraImageRequest cameraRequest(camera, request);
            baseProvider.reset(new CameraThumbnailProvider(cameraRequest));
        }
        else if (request.resource)
        {
            baseProvider.reset(new FfmpegImageProvider(request.resource));
        }
    }

    QScopedPointer<QnImageProvider> baseProvider;
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
        d->baseProvider->disconnect(this);

    d->updateRequest(request);

    if (auto p = d->baseProvider.data())
    {
        connect(p, &QnImageProvider::imageChanged, this, &QnImageProvider::imageChanged);
        connect(p, &QnImageProvider::statusChanged, this, &QnImageProvider::statusChanged);
        connect(p, &QnImageProvider::sizeHintChanged, this, &QnImageProvider::sizeHintChanged);
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
}

} // namespace desktop
} // namespace client
} // namespace nx
