// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_thumbnail_provider.h"

#include <chrono>
#include <limits>

#include <QtCore/QCache>
#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtGui/QIcon>
#include <QtGui/QPainter>

#include <api/server_rest_connection.h>
#include <common/common_module.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <nx/fusion/model_functions.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/image_providers/camera_thumbnail_provider.h>
#include <nx/vms/client/desktop/image_providers/ffmpeg_image_provider.h>
#include <nx/vms/client/desktop/style/style.h>
#include <utils/common/aspect_ratio.h>

namespace nx::vms::client::desktop {

static const QColor kDark8Color = "#263137";
static const core::SvgIconColorer::IconSubstitutions kIconSubstitutions = {
    {QIcon::Normal, {{kDark8Color, "dark8"}}},
};

namespace {

using PlaceholderKey = QPair<QString, QSize>;
static constexpr int kPlaceholderCacheLimit = 16 * 1024 * 1024;

static constexpr qreal kPlaceholderIconFraction = 0.5;

static constexpr QSize kDefaultPlaceholderSize(400, 300);
static const QnAspectRatio kDefaultPlaceholderAspectRatio(kDefaultPlaceholderSize);

using nx::vms::client::core::Geometry;

QSize placeholderSize(const QSize& requestedSize)
{
    enum
    {
        FixedWidth = 0x1,
        FixedHeight = 0x2
    };

    const auto flags = (requestedSize.width() > 0 ? FixedWidth : 0)
        | (requestedSize.height() > 0 ? FixedHeight : 0);

    switch (flags)
    {
        case FixedWidth:
            return Geometry::expanded(kDefaultPlaceholderAspectRatio.toFloat(),
                QSize(requestedSize.width(), std::numeric_limits<int>::max()),
                Qt::KeepAspectRatio).toSize();

        case FixedHeight:
            return Geometry::expanded(kDefaultPlaceholderAspectRatio.toFloat(),
                QSize(std::numeric_limits<int>::max(), requestedSize.height()),
                Qt::KeepAspectRatio).toSize();

        case FixedWidth | FixedHeight:
            return requestedSize;

        default:
            return kDefaultPlaceholderSize;
    }
}

} // namespace

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
            return {ProviderType::image, "item_placeholders/videowall_server_placeholder_400.svg"};

        if (flags.testFlag(Qn::web_page))
            return {
                ProviderType::image, "item_placeholders/videowall_webpage_placeholder_400.svg"};

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

    void updateRequest(
        ResourceThumbnailProvider* q,
        const nx::api::ResourceImageRequest& value,
        bool /*resetStatus*/)
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
                nx::api::CameraImageRequest cameraRequest(camera, request, request.objectTrackId);

                if (auto provider = qobject_cast<CameraThumbnailProvider*>(baseProvider.get()))
                    provider->setRequestData(cameraRequest);
                else
                    baseProvider.reset(new CameraThumbnailProvider(cameraRequest));

                break;
            }

            case ProviderType::ffmpeg:
            {
                baseProvider.reset(new FfmpegImageProvider(value.resource,
                    request.timestampMs, request.size));
                break;
            }

            case ProviderType::image:
            {
                baseProvider.reset(new BasicImageProvider(ensurePlaceholder(placeholderIconPath,
                    placeholderSize(request.size))));
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

    static QImage ensurePlaceholder(const QString& path, const QSize& size)
    {
        if (auto imagePtr = placeholders[{path, size}])
            return *imagePtr;

        const QImage image = createPlaceholder(path, size);
        placeholders.insert({path, size}, new QImage(image), image.width() * image.height());
        return image;
    }

    static QImage createPlaceholder(const QString& path, const QSize& size)
    {
        const auto pixmap = path.endsWith(".svg")
            ? qnSkin->icon(path, kIconSubstitutions).pixmap(size)
            : qnSkin->icon(path).pixmap(size);
        const auto pixelRatio = pixmap.devicePixelRatio();

        QPixmap destination(size * pixelRatio);
        destination.setDevicePixelRatio(pixmap.devicePixelRatio());
        destination.fill(core::colorTheme()->color("dark6"));

        const auto rect = Geometry::aligned(
            Geometry::scaled(pixmap.size() / pixelRatio, size * kPlaceholderIconFraction).toSize(),
            QRect({}, size),
            Qt::AlignCenter);

        QPainter painter(&destination);
        painter.setRenderHints(QPainter::SmoothPixmapTransform);
        painter.setOpacity(0.7);
        painter.drawPixmap(rect, pixmap);
        painter.end();

        return destination.toImage();
    }

    QScopedPointer<ImageProvider> baseProvider;
    nx::api::ResourceImageRequest request;

    static QCache<PlaceholderKey, QImage> placeholders;
};

QCache<PlaceholderKey, QImage> ResourceThumbnailProvider::Private::placeholders(
    kPlaceholderCacheLimit);

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

void ResourceThumbnailProvider::setRequestData(
    const nx::api::ResourceImageRequest& request, bool resetStatus)
{
    d->updateRequest(this, request, resetStatus);
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

bool ResourceThumbnailProvider::tryLoad()
{
    return d->baseProvider ? d->baseProvider->tryLoad() : false;
}

void ResourceThumbnailProvider::doLoadAsync()
{
    if (d->baseProvider)
        d->baseProvider->loadAsync();
}

QnResourcePtr ResourceThumbnailProvider::resource() const
{
    return d->request.resource;
}

std::chrono::microseconds ResourceThumbnailProvider::timestamp() const
{
    if (const auto cameraProvider = qobject_cast<CameraThumbnailProvider*>(d->baseProvider.get()))
        return std::chrono::microseconds(cameraProvider->timestampUs());

    return d->request.timestampMs;
}

} // namespace nx::vms::client::desktop
