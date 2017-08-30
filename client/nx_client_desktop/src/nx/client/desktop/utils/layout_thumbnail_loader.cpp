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

static QRectF boundedRect(const QSizeF& sourceSize, const QRectF& boundingRect)
{
    const auto alignedRect = QnGeometry::aligned(sourceSize, boundingRect);
    return QnGeometry::scaled(alignedRect, boundingRect.size(),
        alignedRect.center(), Qt::KeepAspectRatio);
}

} // namespace

struct LayoutThumbnailLoader::Private
{
    // Input data.
    QnLayoutResourcePtr layout;
    QSize maximumSize;
    qint64 msecSinceEpoch;
    QnThumbnailRequestData::ThumbnailFormat format;

    // Methods.
    Private(const QnLayoutResourcePtr& layout,
        const QSize& maximumSize,
        qint64 msecSinceEpoch,
        QnThumbnailRequestData::ThumbnailFormat format)
        :
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

    void updateTileStatus(Qn::ThumbnailStatus status, const QRectF& tileRect)
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
                // Paint into pixmap:
                QPixmap pixmap(tileRect.size().toSize() * data.image.devicePixelRatio());
                pixmap.setDevicePixelRatio(data.image.devicePixelRatio());
                pixmap.fill(Qt::transparent);
                noDataWidget->setGeometry(tileRect.toRect());
                noDataWidget->render(&pixmap);

                // Paint pixmap into main image:
                QPainter painter(&data.image);
                paintPixmapSharp(&painter, pixmap, tileRect.topLeft());
                --data.numLoading;
                break;
            }
        }
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
        const QRectF scaledCellRect(
            (cellRect.left() - bounding.left() + spacing * 0.5) * xscale,
            (cellRect.top() - bounding.top() + spacing * 0.5) * yscale,
            (cellRect.width() - spacing) * xscale,
            (cellRect.height() - spacing) * yscale);

        QSharedPointer<QnSingleThumbnailLoader> loader(new QnSingleThumbnailLoader(
            camera,
            d->msecSinceEpoch,
            data.rotation,
            QSize(0, scaledCellRect.height()),
            d->format));

        connect(loader.data(), &QnImageProvider::statusChanged,
            [this, loader, scaledCellRect](Qn::ThumbnailStatus status)
            {
                // TODO: #vkutin Use correct item rotation!

                d->updateTileStatus(status, boundedRect(loader->sizeHint(), scaledCellRect));
                if (d->data.numLoading > 0)
                    return;

                d->data.status = Qn::ThumbnailStatus::Loaded;
                emit statusChanged(d->data.status);
            });

        connect(loader.data(), &QnImageProvider::imageChanged,
            [this, loader, scaledCellRect](const QImage& tile)
            {
                if (tile.isNull())
                    return;

                // TODO: #vkutin Use correct item rotation!

                QPainter painter(&d->data.image);
                painter.drawImage(boundedRect(loader->sizeHint(), scaledCellRect), tile);
                painter.end();

                emit imageChanged(d->data.image);
            });

        d->data.loaders.push_back(loader);
        ++d->data.numLoading;

        loader->loadAsync();

        d->data.status = Qn::ThumbnailStatus::Loading;
        emit statusChanged(d->data.status);
    }
}

} // namespace desktop
} // namespace client
} // namespace nx
