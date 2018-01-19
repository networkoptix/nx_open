#include "resource_thumbnail_provider.h"

#include <api/server_rest_connection.h>

#include <nx/client/desktop/image_providers/camera_thumbnail_provider.h>
#include <nx/client/desktop/image_providers/ffmpeg_image_provider.h>

#include <common/common_module.h>

#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>

#include <nx/fusion/model_functions.h>

namespace nx {
namespace client {
namespace desktop {

struct ResourceThumbnailProvider::Private
{
    void updateRequest(const api::ResourceImageRequest& value)
    {
        request = value;
        if (const auto camera = request.resource.dynamicCast<QnVirtualCameraResource>())
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
