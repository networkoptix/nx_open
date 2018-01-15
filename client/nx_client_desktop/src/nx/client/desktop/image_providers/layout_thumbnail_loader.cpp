#include "layout_thumbnail_loader.h"

#include <algorithm>
#include <QtGui/QPainter>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/client/desktop/image_providers/layout_background_image_provider.h>
#include <nx/client/desktop/image_providers/resource_thumbnail_provider.h>

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
static const QSize kBackgroundPreviewSize(200, 200);

// TODO: #vkutin #common Put into utility class/namespace. Get rid of duplicates.
static inline bool isOdd(int value)
{
    return (value & 1) != 0;
}

} // namespace

using ResourceThumbnailProviderPtr = QSharedPointer<ResourceThumbnailProvider>;

struct LayoutThumbnailLoader::Private
{
    // Input data.
    LayoutThumbnailLoader* const q;
    QnLayoutResourcePtr layout;
    QSize maximumSize;
    qint64 msecSinceEpoch;

    // Methods.
    Private(LayoutThumbnailLoader* q,
        const QnLayoutResourcePtr& layout,
        const QSize& maximumSize,
        qint64 msecSinceEpoch)
        :
        q(q),
        layout(layout),
        maximumSize(maximumSize),
        msecSinceEpoch(msecSinceEpoch),
        noDataWidget(new QnAutoscaledPlainText()),
        nonCameraWidget(new QnAutoscaledPlainText())
    {
        noDataWidget->setText(tr("NO DATA"));
        noDataWidget->setProperty(style::Properties::kDontPolishFontProperty, true);
        noDataWidget->setAlignment(Qt::AlignCenter);
        noDataWidget->setContentsMargins(kMinIndicationMargins);

        nonCameraWidget->setText(tr("NOT A CAMERA"));
        nonCameraWidget->setProperty(style::Properties::kDontPolishFontProperty, true);
        nonCameraWidget->setAlignment(Qt::AlignCenter);
        nonCameraWidget->setContentsMargins(kMinIndicationMargins);
        setPaletteColor(nonCameraWidget.data(), QPalette::WindowText, qnGlobals->errorTextColor());
    }

    void updateTileStatus(Qn::ThumbnailStatus status, const QnAspectRatio& aspectRatio,
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

                // Aspect ratio is invalid when there is no image. No need to rotate in this case.
                if (qFuzzyIsNull(rotation) || !aspectRatio.isValid())
                {
                    const auto targetRect = aspectRatio.isValid()
                        ? QnGeometry::expanded(aspectRatio.toFloat(), cellRect, Qt::KeepAspectRatio)
                        : cellRect;

                    QPainter painter(&data.image);
                    paintPixmapSharp(&painter, specialPixmap(status, targetRect.size().toSize()),
                        targetRect.topLeft());
                }
                else
                {
                    if (rotation < 0.0)
                        rotation += 180.0;

                    // 0 = up, 1 = left, 2 = down.
                    const auto direction = qRound(rotation / 90.0);

                    qreal ar = aspectRatio.toFloat();
                    rotation -= direction * 90.0;
                    if (isOdd(direction))
                        ar = 1.0 / ar;

                    const auto cellCenter = cellRect.center();
                    const auto targetRect = QnGeometry::encloseRotatedGeometry(
                        cellRect, ar, rotation);

                    QPainter painter(&data.image);
                    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
                    painter.translate(cellRect.center());
                    painter.rotate(rotation);
                    painter.translate(-cellRect.center());
                    paintPixmapSharp(&painter, specialPixmap(status, targetRect.size().toSize()),
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

    void drawBackground(const QImage& background, const QRectF& targetRect)
    {
        if (background.isNull())
            return;

        const auto loaded = data.image;

        data.image = QImage(loaded.size(), QImage::Format_ARGB32_Premultiplied);
        data.image.fill(Qt::transparent);

        QPainter painter(&data.image);
        painter.setRenderHints(QPainter::SmoothPixmapTransform);
        painter.setOpacity(layout->backgroundOpacity());
        painter.drawImage(targetRect, background, background.rect());
        painter.setOpacity(1.0);
        painter.drawImage(0, 0, loaded);
    }

    QPixmap specialPixmap(Qn::ThumbnailStatus status, const QSize& size) const
    {
        QPixmap pixmap(size * data.image.devicePixelRatio());
        pixmap.setDevicePixelRatio(data.image.devicePixelRatio());
        pixmap.fill(Qt::transparent);

        switch (status)
        {
            case Qn::ThumbnailStatus::NoData:
                noDataWidget->resize(size);
                noDataWidget->render(&pixmap);
                break;

            case Qn::ThumbnailStatus::Invalid:
                nonCameraWidget->resize(size);
                nonCameraWidget->render(&pixmap);
                break;

            default:
                NX_ASSERT(false);
        }

        return pixmap;
    }

    // Intermediate/output data.
    struct Data
    {
        QImage image;
        Qn::ThumbnailStatus status = Qn::ThumbnailStatus::Invalid;
        QList<ResourceThumbnailProviderPtr> loaders;
        int numLoading = 0;
    };

    Data data;
    QScopedPointer<QnAutoscaledPlainText> noDataWidget;
    QScopedPointer<QnAutoscaledPlainText> nonCameraWidget;
};

LayoutThumbnailLoader::LayoutThumbnailLoader(
    const QnLayoutResourcePtr& layout,
    const QSize& maximumSize,
    qint64 msecSinceEpoch,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(this, layout, maximumSize, msecSinceEpoch))
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
    const auto layoutItems = d->layout->getItems();

    QVector<QPair<QnUuid, QnResourcePtr>> validItems;
    validItems.reserve(layoutItems.size());

    for (auto iter = layoutItems.constBegin(); iter != layoutItems.constEnd(); ++iter)
    {
        const auto& itemRect = iter->combinedGeometry;
        if (!itemRect.isValid()) // TODO: #GDM #VW some items can be not placed yet, wtf
            continue;

        const auto resource = resourcePool->getResourceByDescriptor(iter->resource);
        if (!resource)
            continue;

        bounding = bounding.united(itemRect);
        validItems << qMakePair(iter.key(),  resource);
    }

    const bool hasBackground = !d->layout->backgroundImageFilename().isEmpty();
    const auto backgroundRect = d->layout->backgroundRect();

    if (hasBackground)
        bounding = bounding.united(backgroundRect);

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

    if (hasBackground)
    {
        auto backgroundLoader = new LayoutBackgroundImageProvider(d->layout,
            kBackgroundPreviewSize, this);
        connect(backgroundLoader, &QnImageProvider::imageChanged, this,
            [this, backgroundRect, xscale, yscale, bounding](const QImage& image)
            {
                const QRectF scaledBackgroundRect(
                    (backgroundRect.left() - bounding.left()) * xscale,
                    (backgroundRect.top() - bounding.top()) * yscale,
                    backgroundRect.width() * xscale,
                    backgroundRect.height() * yscale);

                d->drawBackground(image, scaledBackgroundRect);
            });
        backgroundLoader->loadAsync();
    }

    for (const auto& item: validItems)
    {
        const auto data = d->layout->getItem(item.first);
        const auto& resource = item.second;
        const auto& cellRect = data.combinedGeometry;
        const auto rotation = data.rotation;
        const auto zoomRect = data.zoomRect;

        // Cell bounds.
        const QRectF scaledCellRect(
            (cellRect.left() - bounding.left() + spacing * 0.5) * xscale,
            (cellRect.top() - bounding.top() + spacing * 0.5) * yscale,
            (cellRect.width() - spacing) * xscale,
            (cellRect.height() - spacing) * yscale);

        QSize thumbnailSize(0, scaledCellRect.height());
        if (!zoomRect.isEmpty())
            thumbnailSize /= zoomRect.height();

        api::ResourceImageRequest request;
        request.resource = resource;
        request.msecSinceEpoch = d->msecSinceEpoch;
        request.size = thumbnailSize;

        ResourceThumbnailProviderPtr loader(new ResourceThumbnailProvider(request));

        connect(loader.data(), &QnImageProvider::statusChanged, this,
            [this, loader, rotation, scaledCellRect](Qn::ThumbnailStatus status)
            {
                QnAspectRatio aspectRatio(loader->sizeHint());
                d->updateTileStatus(status, aspectRatio, scaledCellRect, rotation);
            });

        connect(loader.data(), &QnImageProvider::imageChanged, this,
            [this, loader, rotation, scaledCellRect, zoomRect](const QImage& tile)
            {
                const auto sourceAr = QnGeometry::aspectRatio(loader->sizeHint());
                auto aspectRatio = zoomRect.isNull()
                    ? sourceAr
                    : sourceAr * QnGeometry::aspectRatio(zoomRect);

                d->drawTile(tile, aspectRatio, scaledCellRect, rotation, zoomRect);
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
