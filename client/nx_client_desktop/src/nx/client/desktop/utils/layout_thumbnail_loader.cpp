#include "layout_thumbnail_loader.h"

#include <algorithm>
#include <QtGui/QPainter>

#include <camera/single_thumbnail_loader.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <ui/common/geometry.h>
#include <ui/style/globals.h>

namespace nx {
namespace client {
namespace desktop {

struct LayoutThumbnailLoader::Private
{
    // Input data.
    QnLayoutResourcePtr layout;
    QSize maximumSize;
    qint64 msecSinceEpoch;
    QnThumbnailRequestData::ThumbnailFormat format;

    Private(const QnLayoutResourcePtr& layout,
        const QSize& maximumSize,
        qint64 msecSinceEpoch,
        QnThumbnailRequestData::ThumbnailFormat format)
        :
        layout(layout),
        maximumSize(maximumSize),
        msecSinceEpoch(msecSinceEpoch),
        format(format)
    {
    }

    // Intermediate/output data.
    struct Data
    {
        QImage image;
        Qn::ThumbnailStatus status = Qn::ThumbnailStatus::Invalid;
        QList<QSharedPointer<QnSingleThumbnailLoader>> loaders;
        int numLoading = 0;
    };

    Data data;
};

LayoutThumbnailLoader::LayoutThumbnailLoader(
    const QnLayoutResourcePtr& layout,
    const QSize maximumSize,
    qint64 msecSinceEpoch,
    QnThumbnailRequestData::ThumbnailFormat format,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(layout, maximumSize, msecSinceEpoch, format))
{
}

LayoutThumbnailLoader::~LayoutThumbnailLoader()
{
}

QnLayoutResourcePtr LayoutThumbnailLoader::layout() const
{
    return d->layout;
}

QImage LayoutThumbnailLoader::image() const
{
    return d->data.image;
}

QSize LayoutThumbnailLoader::sizeHint() const
{
    return d->data.image.size();
}

Qn::ThumbnailStatus LayoutThumbnailLoader::status() const
{
    return d->data.status;
}

void LayoutThumbnailLoader::doLoadAsync()
{
    if (!d->layout)
        return;

    const auto resourcePool = d->layout->resourcePool();
    if (!resourcePool)
        return;

    d->data = Private::Data();

    QRectF bounding;
    for (const auto& data: d->layout->getItems())
    {
        const QRectF itemRect = data.combinedGeometry;
        if (itemRect.isValid()) // TODO: #GDM #VW some items can be not placed yet, wtf
            bounding = bounding.united(itemRect);
    }

    if (bounding.isEmpty())
    {
        d->data.status = Qn::ThumbnailStatus::Loaded;
        emit sizeHintChanged(QSize());
        emit statusChanged(d->data.status);
        emit imageChanged(d->data.image);
        return;
    }

    // Handle negative spacing for exported layouts.
    const qreal spacing = std::max(0.0, d->layout->cellSpacing());

    const qreal cellAspectRatio = d->layout->hasCellAspectRatio()
        ? d->layout->cellAspectRatio()
        : qnGlobals->defaultLayoutCellAspectRatio();

    qreal xscale, yscale;
    const qreal sourceAr = cellAspectRatio * bounding.width() / bounding.height();

    const qreal targetAr = QnGeometry::aspectRatio(d->maximumSize);
    if (sourceAr > targetAr)
    {
        xscale = d->maximumSize.width() / bounding.width();
        yscale = xscale / cellAspectRatio;
    }
    else
    {
        yscale = d->maximumSize.height() / bounding.height();
        xscale = yscale * cellAspectRatio;
    }

    d->data.image = QImage(QSize(bounding.width() * xscale, bounding.height() * yscale),
        QImage::Format_ARGB32_Premultiplied);

    d->data.image.fill(Qt::transparent);

    emit sizeHintChanged(d->data.image.size());

    for (const auto& data: d->layout->getItems())
    {
        const QRectF cellRect = data.combinedGeometry;
        if (!cellRect.isValid())
            continue;

        const auto resource = resourcePool->getResourceByDescriptor(data.resource);
        if (!resource)
            continue;

        // TODO: #vkutin Do we want to draw placeholders for non-camera resources?
        const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
        if (!camera)
            continue;

        // Cell bounds.
        const qreal x = (cellRect.left() - bounding.left() + spacing * 0.5) * xscale;
        const qreal y = (cellRect.top() - bounding.top() + spacing * 0.5) * yscale;
        const qreal w = (cellRect.width() - spacing) * xscale;
        const qreal h = (cellRect.height() - spacing) * yscale;

        QSharedPointer<QnSingleThumbnailLoader> loader(new QnSingleThumbnailLoader(
            camera,
            d->msecSinceEpoch,
            data.rotation,
            QSize(w, 0),
            d->format));

        connect(loader.data(), &QnImageProvider::statusChanged,
            [this](Qn::ThumbnailStatus status)
            {
                if (status == Qn::ThumbnailStatus::Loading)
                    return;

                if (--d->data.numLoading > 0)
                    return;

                d->data.status = Qn::ThumbnailStatus::Loaded;
                emit statusChanged(d->data.status);
            });

        connect(loader.data(), &QnImageProvider::imageChanged,
            [this, rect = QRectF(x, y, w, h)](const QImage& image)
            {
                if (image.isNull())
                    return;

                const auto imageRect = QnGeometry::aligned(image.size(), rect);
                const auto fitRect = QnGeometry::scaled(imageRect, rect.size(),
                    imageRect.center(), Qt::KeepAspectRatio);

                QPainter painter(&d->data.image);
                painter.drawImage(fitRect, image);
                painter.end();

                emit imageChanged(d->data.image);
            });

        d->data.loaders.push_back(loader);
        ++d->data.numLoading;

        loader->loadAsync();
    }
}

} // namespace desktop
} // namespace client
} // namespace nx
