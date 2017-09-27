#include "layout_thumbnail_loader.h"

#include <algorithm>
#include <QtGui/QPainter>

#include <camera/single_thumbnail_loader.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <ui/common/geometry.h>
#include <ui/common/palette.h>
#include <ui/style/globals.h>
#include <ui/style/helper.h>
#include <ui/widgets/common/autoscaled_plain_text.h>
#include <ui/workaround/sharp_pixmap_painting.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

static const QMargins kMinIndicationMargins(4, 2, 4, 2);

// TODO: #vkutin #common Put into utility class/namespace. Get rid of duplicates.
static inline bool isOdd(int value)
{
    return (value & 1) != 0;
}

} // namespace

struct LayoutThumbnailLoader::Private
{
    // Input data.
    LayoutThumbnailLoader* const q;
    QnLayoutResourcePtr layout;
    QSize maximumSize;
    qint64 msecSinceEpoch;
    QnThumbnailRequestData::ThumbnailFormat format;

    // Methods.
    Private(LayoutThumbnailLoader* q,
        const QnLayoutResourcePtr& layout,
        const QSize& maximumSize,
        qint64 msecSinceEpoch,
        QnThumbnailRequestData::ThumbnailFormat format)
        :
        q(q),
        layout(layout),
        maximumSize(maximumSize),
        msecSinceEpoch(msecSinceEpoch),
        format(format),
        noDataWidget(new QnAutoscaledPlainText())
    {
        noDataWidget->setText(tr("NO DATA"));
        noDataWidget->setProperty(style::Properties::kDontPolishFontProperty, true);
        noDataWidget->setAlignment(Qt::AlignCenter);
        noDataWidget->setContentsMargins(kMinIndicationMargins);
    }

    void updateTileStatus(Qn::ThumbnailStatus status, qreal aspectRatio,
        const QRectF& cellRect, qreal rotation)
    {
        switch (status)
        {
            case Qn::ThumbnailStatus::Loading:
                break;

            case Qn::ThumbnailStatus::Loaded:
                --data.numLoading;
                break;

            case Qn::ThumbnailStatus::Refreshing:
            case Qn::ThumbnailStatus::Invalid:
            case Qn::ThumbnailStatus::NoData:
            {
                rotation = qMod(rotation, 180.0);

                if (qFuzzyIsNull(rotation))
                {
                    const auto targetRect = QnGeometry::expanded(aspectRatio,
                        cellRect, Qt::KeepAspectRatio);

                    QPainter painter(&data.image);
                    paintPixmapSharp(&painter, noDataPixmap(targetRect.size().toSize()),
                        targetRect.topLeft());
                }
                else
                {
                    if (rotation < 0.0)
                        rotation += 180.0;

                    // 0 = up, 1 = left, 2 = down.
                    const auto direction = qRound(rotation / 90.0);

                    rotation -= direction * 90.0;
                    if (isOdd(direction))
                        aspectRatio = 1.0 / aspectRatio;

                    const auto cellCenter = cellRect.center();
                    const auto targetRect = QnGeometry::encloseRotatedGeometry(
                        cellRect, aspectRatio, rotation);

                    QPainter painter(&data.image);
                    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
                    painter.translate(cellRect.center());
                    painter.rotate(rotation);
                    painter.translate(-cellRect.center());
                    paintPixmapSharp(&painter, noDataPixmap(targetRect.size().toSize()),
                        targetRect.topLeft());
                }

                emit q->imageChanged(data.image);

                --data.numLoading;
                break;
            }
        }

        if (data.numLoading > 0)
            return;

        data.status = Qn::ThumbnailStatus::Loaded;
        emit q->statusChanged(data.status);
    }

    void drawTile(const QImage& tile, qreal aspectRatio, const QRectF& cellRect,
        qreal rotation, const QRectF& zoomRect)
    {
        if (tile.isNull())
            return;

        const auto sourceRect = zoomRect.isNull()
            ? tile.rect()
            : QnGeometry::subRect(tile.rect(), zoomRect).toRect();

        if (qFuzzyIsNull(rotation))
        {
            const auto targetRect = QnGeometry::expanded(aspectRatio,
                cellRect, Qt::KeepAspectRatio);

            QPainter painter(&data.image);
            painter.setRenderHints(QPainter::SmoothPixmapTransform);
            painter.drawImage(targetRect, tile, sourceRect);
        }
        else
        {
            const auto cellCenter = cellRect.center();
            const auto targetRect = QnGeometry::encloseRotatedGeometry(
                cellRect, aspectRatio, rotation);

            QPainter painter(&data.image);
            painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
            painter.translate(cellRect.center());
            painter.rotate(rotation);
            painter.translate(-cellRect.center());
            painter.drawImage(targetRect, tile, sourceRect);
        }

        emit q->imageChanged(data.image);
    }

    QPixmap noDataPixmap(const QSize& size) const
    {
        QPixmap pixmap(size * data.image.devicePixelRatio());
        pixmap.setDevicePixelRatio(data.image.devicePixelRatio());
        pixmap.fill(Qt::transparent);
        noDataWidget->resize(size);
        noDataWidget->render(&pixmap);
        return pixmap;
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
    QScopedPointer<QnAutoscaledPlainText> noDataWidget;
};

LayoutThumbnailLoader::LayoutThumbnailLoader(
    const QnLayoutResourcePtr& layout,
    const QSize maximumSize,
    qint64 msecSinceEpoch,
    QnThumbnailRequestData::ThumbnailFormat format,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(this, layout, maximumSize, msecSinceEpoch, format))
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

QColor LayoutThumbnailLoader::itemBackgroundColor() const
{
    return d->noDataWidget->palette().color(QPalette::Window);
}

void LayoutThumbnailLoader::setItemBackgroundColor(const QColor& value)
{
    setPaletteColor(d->noDataWidget.data(), QPalette::Window, value);
}

QColor LayoutThumbnailLoader::fontColor() const
{
    return d->noDataWidget->palette().color(QPalette::WindowText);
}

void LayoutThumbnailLoader::setFontColor(const QColor& value)
{
    setPaletteColor(d->noDataWidget.data(), QPalette::WindowText, value);
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
    d->data.status = Qn::ThumbnailStatus::Loading;

    emit sizeHintChanged(d->data.image.size());
    emit statusChanged(d->data.status);

    for (const auto& data : d->layout->getItems())
    {
        const QRectF cellRect = data.combinedGeometry;
        if (!cellRect.isValid())
            continue;

        const auto resource = resourcePool->getResourceByDescriptor(data.resource);
        if (!resource)
            continue;

        // Cell bounds.
        const QRectF scaledCellRect(
            (cellRect.left() - bounding.left() + spacing * 0.5) * xscale,
            (cellRect.top() - bounding.top() + spacing * 0.5) * yscale,
            (cellRect.width() - spacing) * xscale,
            (cellRect.height() - spacing) * yscale);

        const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
        if (!camera)
        {
            // Non-camera resources are drawn as "NO DATA".
            const auto scaledCellAr = QnGeometry::aspectRatio(scaledCellRect);
            d->updateTileStatus(Qn::ThumbnailStatus::NoData, scaledCellAr, scaledCellRect, 0);
            continue;
        }

        const auto rotation = data.rotation;
        const auto zoomRect = data.zoomRect;

        QSize thumbnailSize(0, scaledCellRect.height());
        if (!zoomRect.isEmpty())
            thumbnailSize /= zoomRect.height();

        QSharedPointer<QnSingleThumbnailLoader> loader(new QnSingleThumbnailLoader(
            camera, d->msecSinceEpoch, 0, thumbnailSize, d->format));

        connect(loader.data(), &QnImageProvider::statusChanged,
            [this, loader, rotation, scaledCellRect](Qn::ThumbnailStatus status)
            {
                d->updateTileStatus(status, QnGeometry::aspectRatio(loader->sizeHint()),
                    scaledCellRect, rotation);
            });

        connect(loader.data(), &QnImageProvider::imageChanged,
            [this, loader, rotation, scaledCellRect, zoomRect](const QImage& tile)
            {
                d->drawTile(tile, QnGeometry::aspectRatio(loader->sizeHint()),
                    scaledCellRect, rotation, zoomRect);
            });

        d->data.loaders.push_back(loader);
        ++d->data.numLoading;

        loader->loadAsync();
    }

    if (d->data.numLoading)
        return;

    // If there's nothing to load.
    d->data.status = Qn::ThumbnailStatus::Loaded;
    emit statusChanged(d->data.status);
}

} // namespace desktop
} // namespace client
} // namespace nx
