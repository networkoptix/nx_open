#include "resource_thumbnail_provider.h"

#include <chrono>

#include <QtGui/QPainter>

#include <api/server_rest_connection.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/avi/avi_resource.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/style/nx_style.h>

#include <nx/fusion/model_functions.h>
#include <nx/vms/client/desktop/image_providers/camera_thumbnail_provider.h>
#include <nx/vms/client/desktop/image_providers/ffmpeg_image_provider.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>

namespace nx::vms::client::desktop {

using namespace std::chrono;

struct ResourceThumbnailProvider::Private
{
    enum class ProviderType
    {
        none,
        camera,
        ffmpeg,
        image
    };

    static std::pair<ProviderType, QString> getRequiredProvider(const QnResourcePtr& resource)
    {
        if (!NX_ASSERT(resource))
            return {ProviderType::none, {}};

        const auto flags = resource->flags();

        if (flags.testFlag(Qn::server))
            return {ProviderType::image, "item_placeholders/videowall_server_placeholder.png"};

        if (flags.testFlag(Qn::web_page))
            return {ProviderType::image, "item_placeholders/videowall_webpage_placeholder.png"};

        // Some cameras are actually provide only sound stream. So we draw sound icon for this.
        const auto mediaResource = resource.dynamicCast<QnMediaResource>();
        if (const bool useCustomSoundIcon = mediaResource && !mediaResource->hasVideo())
            return {ProviderType::image, "item_placeholders/sound.png"};

        if (resource.dynamicCast<QnVirtualCameraResource>())
            return {ProviderType::camera, {}};

        if (resource.dynamicCast<QnAviResource>())
            return {ProviderType::ffmpeg, {}};

        NX_ASSERT(false);
        return {ProviderType::none, {}};
    }

    void updateRequest(ResourceThumbnailProvider* q, const nx::api::ResourceImageRequest& value)
    {
        const auto [providerType, placeholderIconPath] = getRequiredProvider(value.resource);
        request = value;

        const auto prevProvider = baseProvider.get();
        const bool wasValid = q->status() != Qn::ThumbnailStatus::Invalid;

        switch (providerType)
        {
            case ProviderType::camera:
            {
                const auto camera = value.resource.dynamicCast<QnVirtualCameraResource>();
                NX_ASSERT(camera);
                nx::api::CameraImageRequest cameraRequest(camera, request);
                cameraRequest.streamSelectionMode = streamSelectionMode;

                if (auto provider = qobject_cast<CameraThumbnailProvider*>(baseProvider.get()))
                    provider->setRequestData(cameraRequest);
                else
                    baseProvider.reset(new CameraThumbnailProvider(cameraRequest));

                break;
            }

            case ProviderType::ffmpeg:
            {
                baseProvider.reset(new FfmpegImageProvider(value.resource,
                    microseconds(request.usecSinceEpoch), request.size));

                break;
            }

            case ProviderType::image:
            {
                const auto pixmap = qnSkin->pixmap(placeholderIconPath, true);

                QPixmap destination(pixmap.size());
                destination.fill(colorTheme()->color("dark4"));

                QPainter painter(&destination);
                painter.setRenderHints(QPainter::SmoothPixmapTransform);
                painter.setOpacity(0.7);
                painter.drawPixmap(0, 0, pixmap);
                painter.end();

                baseProvider.reset(new BasicImageProvider(destination.toImage()));
                break;
            }

            case ProviderType::none:
            {
                baseProvider.reset();
                break;
            }
        }

        if (prevProvider != baseProvider.get())
        {
            if (baseProvider)
            {
                QObject::connect(baseProvider.data(), &ImageProvider::imageChanged,
                    q, &ImageProvider::imageChanged);
                QObject::connect(baseProvider.data(), &ImageProvider::statusChanged,
                    q, &ImageProvider::statusChanged);
                QObject::connect(baseProvider.data(), &ImageProvider::sizeHintChanged,
                    q, &ImageProvider::sizeHintChanged);
            }

            if (wasValid)
            {
                emit q->imageChanged(q->image());
                emit q->sizeHintChanged(q->sizeHint());
                emit q->statusChanged(q->status());
            }
        }
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
    d->updateRequest(this, request);
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
