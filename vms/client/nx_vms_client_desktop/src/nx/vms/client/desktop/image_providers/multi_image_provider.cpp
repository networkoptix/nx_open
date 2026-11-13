// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "multi_image_provider.h"

#include <QtGui/QPainter>

#include <client/client_globals.h>
#include <nx/vms/client/core/utils/geometry.h>

using nx::vms::client::core::Geometry;

namespace { static const QSize kMaxSize(400, 160); } // namespace

namespace nx::vms::client::desktop {

MultiImageProvider::MultiImageProvider(
    Providers providers,
    Qt::Orientation orientation,
    int spacing,
    QObject* parent)
    :
    base_type(parent),
    m_providers(std::move(providers)),
    m_orientation(orientation),
    m_spacing(spacing)
{
    int key = 0;
    for (const auto& provider: m_providers)
    {
        connect(provider.get(), &ImageProvider::imageChanged, this,
            [this, key](const QImage &loadedImage)
            {
                if (loadedImage.isNull())
                    return;

                // Images are laid out in the provider order, whatever order they load in.
                m_images[key] = loadedImage;
                updateImage();

                emit sizeHintChanged(sizeHint());
                emit imageChanged(m_image);
            });

        connect(provider.get(), &ImageProvider::statusChanged, this,
            [this] {emit statusChanged(status()); });
        connect(provider.get(), &ImageProvider::sizeHintChanged, this,
            [this] { emit sizeHintChanged(sizeHint()); });

        ++key;
    }
}

void MultiImageProvider::updateImage()
{
    QMap<int, QRect> rects;
    QSize size;
    for (const auto& [key, image]: m_images.asKeyValueRange())
    {
        const QSize imageSize =
            Geometry::bounded(image.size(), kMaxSize, Qt::KeepAspectRatio).toSize();

        QRect rect(QPoint(0, 0), imageSize);
        if (!size.isEmpty())
        {
            if (m_orientation == Qt::Vertical)
                rect.translate(0, size.height() + m_spacing);
            else
                rect.translate(size.width() + m_spacing, 0);
        }

        rects[key] = rect;
        size = size.expandedTo(QSize(rect.right() + 1, rect.bottom() + 1));
    }

    QImage mergedImage(size, QImage::Format_ARGB32_Premultiplied);
    mergedImage.fill(Qt::transparent);

    QPainter painter(&mergedImage);
    for (const auto& [key, image]: m_images.asKeyValueRange())
        painter.drawImage(rects.value(key), image);
    painter.end();

    m_image = mergedImage;
    m_imageRects = rects;
}

QMap<int, QRect> MultiImageProvider::imageRects() const
{
    return m_imageRects;
}

QImage MultiImageProvider::image() const
{
    return m_image;
}

QSize MultiImageProvider::sizeHint() const
{
    if (!m_image.isNull())
        return m_image.size();

    if (m_providers.empty())
        return QSize();

    return Geometry::bounded(m_providers.front()->sizeHint(), kMaxSize, Qt::KeepAspectRatio).toSize();
}

core::ThumbnailStatus MultiImageProvider::status() const
{
    // TODO: #sivanov Improve logic, handle statusChanged.
    for (const auto& provider: m_providers)
    {
        if (provider->status() == core::ThumbnailStatus::Loaded)
            return core::ThumbnailStatus::Loaded;
    }
    return core::ThumbnailStatus::Loading;
}

void MultiImageProvider::doLoadAsync()
{
    for (const auto& provider: m_providers)
        provider->loadAsync();
}

} // namespace nx::vms::client::desktop
