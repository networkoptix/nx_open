// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_thumbnail_loader.h"

#include <algorithm>
#include <chrono>

#include <QtGui/QPainter>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>

#include <core/resource/avi/avi_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/log.h>
#include <nx/utils/math/math.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/common/widgets/autoscaled_plain_text.h>
#include <nx/vms/client/desktop/common/widgets/scalable_image_widget.h>
#include <nx/vms/client/desktop/image_providers/layout_background_image_provider.h>
#include <nx/vms/client/desktop/image_providers/resource_thumbnail_provider.h>
#include <nx/vms/client/desktop/image_providers/watermark_proxy_provider.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/resource/resource_descriptor.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <ui/common/palette.h>
#include <ui/workaround/sharp_pixmap_painting.h>

using nx::vms::client::core::Geometry;

namespace nx::vms::client::desktop {

namespace {

static const QMargins kMinIndicationMargins(4, 2, 4, 2);
static const QSize kBackgroundPreviewSize(200, 200);

// TODO: #vkutin Put into utility class/namespace. Get rid of duplicates.
static inline bool isOdd(int value)
{
    return (value & 1) != 0;
}

bool hasExportPermission(const QnResourcePtr& resource)
{
    const auto accessController = SystemContext::fromResource(resource)->accessController();
    return accessController->hasPermissions(resource, Qn::ExportPermission);
}

} // namespace

using ResourceThumbnailProviderPtr = QSharedPointer<ResourceThumbnailProvider>;
using ImageProviderPtr = QSharedPointer<ImageProvider>;

struct LayoutThumbnailLoader::Private
{
    // Input data.
    LayoutThumbnailLoader* const q;
    QnLayoutResourcePtr layout;
    QSize maximumSize;
    qint64 msecSinceEpoch;
    bool skipExportPermissionCheck = false;

    // Pretty name for debug output.
    QString layoutName;

    nx::core::Watermark watermark;

    struct ThumbnailItemData
    {
        QString name;
        // Resource to be loaded. Can be empty when we load a background
        QnResourcePtr resource;
        // Image provider that deals with loading process
        ImageProviderPtr provider;
        // Last status for an item
        Qn::ThumbnailStatus status;
        // Desired rotation for a tile.
        qreal rotation;
        // Output rectangle for the cell, that contains this resource
        QRectF outCellRect;
        // Final output rectangle
        QRect outRect;

        bool rendered = false;
    };

    // Because we share this items.
    using ItemPtr = std::shared_ptr<ThumbnailItemData>;

    // Methods.
    Private(LayoutThumbnailLoader* q,
        const QnLayoutResourcePtr& layout,
        const QSize& maximumSize,
        qint64 msecSinceEpoch,
        bool skipExportPermissionCheck)
        :
        q(q),
        layout(layout),
        maximumSize(maximumSize),
        msecSinceEpoch(msecSinceEpoch),
        skipExportPermissionCheck(skipExportPermissionCheck),
        noDataWidget(new AutoscaledPlainText()),
        offlineWidget(new QWidget()),
        noExportPermissionWidget(new QWidget()),
        nonCameraWidget(new AutoscaledPlainText())
    {
        QFont font;
        font.setPixelSize(88);
        font.setWeight(QFont::Light);

        noDataWidget->setText(ini().debugDisableCameraThumbnails ? tr("DISABLED") : tr("NO DATA"));
        noDataWidget->setProperty(style::Properties::kDontPolishFontProperty, true);
        noDataWidget->setContentsMargins(kMinIndicationMargins);
        noDataWidget->setFont(font);

        auto offlineWidgetLayout = new QVBoxLayout(offlineWidget.get());
        offlineWidgetLayout->setSpacing(0);
        offlineWidgetLayout->setContentsMargins(6, 2, 6, 6);
        auto noSignalImage = new ScalableImageWidget(":/skin/item_placeholders/offline.svg");
        offlineWidgetLayout->addWidget(noSignalImage, 3, Qt::AlignHCenter);

        auto noSignalText = new AutoscaledPlainText();
        noSignalText->setText(tr("OFFLINE"));
        noSignalText->setProperty(style::Properties::kDontPolishFontProperty, true);
        noSignalText->setContentsMargins(kMinIndicationMargins);
        noSignalText->setFont(font);
        setPaletteColor(noSignalText, QPalette::WindowText, core::colorTheme()->color("red_core"));
        offlineWidgetLayout->addWidget(noSignalText, 1, Qt::AlignHCenter);

        auto noExportPermissionLayout = new QVBoxLayout(noExportPermissionWidget.get());
        noExportPermissionLayout->setSpacing(0);
        noExportPermissionLayout->setContentsMargins(6, 2, 6, 6);
        noExportPermissionLayout->addWidget(new ScalableImageWidget(
            ":/skin/item_placeholders/no_access.svg"), 3, Qt::AlignHCenter);

        auto noExportPermissionText = new AutoscaledPlainText();
        noExportPermissionText->setText(tr("NO EXPORT PERMISSION"));
        noExportPermissionText->setProperty(style::Properties::kDontPolishFontProperty, true);
        noExportPermissionText->setContentsMargins(kMinIndicationMargins);
        noExportPermissionText->setFont(font);
        setPaletteColor(noExportPermissionText,
            QPalette::WindowText,
            core::colorTheme()->color("red_core"));
        noExportPermissionLayout->addWidget(noExportPermissionText, 1, Qt::AlignHCenter);

        nonCameraWidget->setText(tr("NOT A CAMERA"));
        nonCameraWidget->setProperty(style::Properties::kDontPolishFontProperty, true);
        nonCameraWidget->setContentsMargins(kMinIndicationMargins);
        setPaletteColor(nonCameraWidget.data(), QPalette::WindowText,
            core::colorTheme()->color("red_core"));
        nonCameraWidget->setFont(font);
    }

    // ItemPtr trackLoader(const QnResourcePtr& resource, ImageProviderPtr loader)
    // Allocate a new item to be tracked.
    ItemPtr newItem()
    {
        ItemPtr item(new ThumbnailItemData());
        items.push_back(item);
        numLoading++;
        return item;
    }

    void notifyLoaderIsComplete()
    {
        if (numLoading > 0)
        {
            --numLoading;
            if(numLoading == 0)
            {
                status = Qn::ThumbnailStatus::Loaded;
                NX_VERBOSE(this) << "LayoutThumbnailLoader(" << layoutName << ") is complete";
                emit q->statusChanged(status);
            }
        }
    }

    // Draws a pixmap for 'NoData' or 'Invalid' state.
    void drawStatusPixmap(Qn::ThumbnailStatus status, const QnAspectRatio& aspectRatio,
        ItemPtr item)
    {
        qreal rotation = qMod(item->rotation, 180.0);
        QRectF cellRect = item->outCellRect;

        const bool exportAllowed = skipExportPermissionCheck || hasExportPermission(item->resource);

        // Aspect ratio is invalid when there is no image. No need to rotate in this case.
        if (qFuzzyIsNull(rotation) || !aspectRatio.isValid())
        {
            if (aspectRatio.isValid())
                item->outRect = Geometry::expanded(
                    aspectRatio.toFloat(), cellRect, Qt::KeepAspectRatio).toRect();
            else
                item->outRect = cellRect.toRect();

            QPainter painter(&outputImage);
            paintPixmapSharp(&painter,
                specialPixmap(
                    item->resource->isOnline(), exportAllowed, status, item->outRect.size()),
                item->outRect.topLeft());
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

            item->outRect = Geometry::encloseRotatedGeometry(
                cellRect, ar, rotation).toRect();

            QPainter painter(&outputImage);
            painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
            painter.translate(cellRect.center());
            painter.rotate(rotation);
            painter.translate(-cellRect.center());
            paintPixmapSharp(&painter,
                specialPixmap(
                    item->resource->isOnline(), exportAllowed, status, item->outRect.size()),
                item->outRect.topLeft());
        }

        finalizeOutputImage();
    }

    void updateTileStatus(Qn::ThumbnailStatus status, const QnAspectRatio& aspectRatio,
        ItemPtr item)
    {
        bool loaderIsComplete = false;
        const auto strStatus = nx::reflect::enumeration::toString(status);

        NX_VERBOSE(this, "LayoutThumbnailLoader(%1) item = %2 status = %3",
            layoutName, item->name, strStatus);

        switch (status)
        {
            case Qn::ThumbnailStatus::Loading:
                break;

            case Qn::ThumbnailStatus::Loaded:
                item->status = status;
                loaderIsComplete = true;
                break;

            case Qn::ThumbnailStatus::Invalid:
            case Qn::ThumbnailStatus::NoData:
                // We try not to change status if we had already 'Loaded
                if(item->status != Qn::ThumbnailStatus::Loaded)
                    drawStatusPixmap(status, aspectRatio, item);
                else
                    item->status = status;
                loaderIsComplete = true;
                break;
            case Qn::ThumbnailStatus::Refreshing:
                break;
        }

        if (loaderIsComplete)
            notifyLoaderIsComplete();
    }

    void finalizeOutputImage()
    {
        QRect selection = this->calculateContentRect();
        if (!selection.isEmpty())
        {
            outputRegion = outputImage.copy(selection);
            emit q->imageChanged(outputRegion);
        }
        else
        {
            NX_WARNING(this) << "LayoutThumbnailLoader::finalizeOutputImage() has empty contents rectangle";
        }
    }

    void drawTile(const ItemPtr& item, qreal aspectRatio, const QRectF& zoomRect)
    {
        const QImage& tile = item->provider->image();

        if (tile.isNull())
            return;

        QRectF sourceRect = tile.rect();
        if (!zoomRect.isNull())
            sourceRect = Geometry::subRect(tile.rect(), zoomRect).toRect();

        if (qFuzzyIsNull(item->rotation))
        {
            item->outRect = Geometry::expanded(
                aspectRatio, item->outCellRect, Qt::KeepAspectRatio).toRect();
            QPainter painter(&outputImage);
            painter.setRenderHints(QPainter::SmoothPixmapTransform);
            painter.drawImage(item->outRect, tile, sourceRect);
        }
        else
        {
            QPainter painter(&outputImage);
            painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
            // We will rotate around center of cell rectangle.
            painter.translate(item->outCellRect.center());
            painter.rotate(item->rotation);
            // Now we should deal with fitting our image to cell rectangle
            // First, we need the size of bounding box of rotate image.
            // We will scale it to fit to cell rectangle.
            QSizeF rotatedSize = painter.transform().mapRect(sourceRect).size();
            // Determine upscaled size of this item on output image.
            QSizeF scaledSize =
                Geometry::expanded(rotatedSize, item->outCellRect.size(), Qt::KeepAspectRatio);
            scaledSize =
                Geometry::bounded(scaledSize, item->outCellRect.size(), Qt::KeepAspectRatio);
            // That's the scale, we should apply to make source image fit.
            qreal scale = Geometry::scaleFactor(rotatedSize, scaledSize, Qt::KeepAspectRatio);
            painter.scale(scale, scale);
            painter.translate(-sourceRect.width()*0.5, -sourceRect.height()*0.5);
            // Now we should have a transform, that lands source image
            // with proper alignment to bounds of the cell.
            painter.drawImage(QPoint(0, 0), tile, sourceRect);
            QRectF adjustedSrcRect(QPoint(0, 0), sourceRect.size());
            // Transfomin' like a boss!
            item->outRect = painter.transform().mapRect(adjustedSrcRect).toRect();

            // Hint: the rect from QnGeometry:: encloseRotatedGeometry is not
            // pixel-accurate bounding box. It will be cropped for rotations around 90degrees
        }

        finalizeOutputImage();
    }

    void drawBackground(const ItemPtr& item, const QImage& background)
    {
        if (background.isNull())
            return;

        // We store current contents.
        // Then we draw background over empty image
        // and after that we finally draw saved contents.
        const QImage loaded = outputImage;

        outputImage = QImage(loaded.size(), QImage::Format_ARGB32_Premultiplied);
        outputImage.fill(Qt::transparent);

        QPainter painter(&outputImage);
        painter.setRenderHints(QPainter::SmoothPixmapTransform);
        painter.setOpacity(layout->backgroundOpacity());
        painter.drawImage(item->outCellRect, background, background.rect());
        item->outRect = item->outCellRect.toRect();
        painter.setOpacity(1.0);
        // Draw old contents over the background.
        painter.drawImage(0, 0, loaded);

        finalizeOutputImage();
    }

    // Generates a pixmap for 'NoData' or 'Invalid' state.
    QPixmap specialPixmap(
        bool isOnline, bool exportAllowed, Qn::ThumbnailStatus status, const QSize& size) const
    {
        QPixmap pixmap(size * outputImage.devicePixelRatio());
        pixmap.setDevicePixelRatio(outputImage.devicePixelRatio());
        pixmap.fill(Qt::transparent);

        auto noDataStatusWidget = exportAllowed
            ? (isOnline || msecSinceEpoch != DATETIME_NOW)
                ? noDataWidget.get()
                : offlineWidget.get()
            : noExportPermissionWidget.get();

        switch (status)
        {
            case Qn::ThumbnailStatus::NoData:
                noDataStatusWidget->resize(size);
                noDataStatusWidget->render(&pixmap);
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

    // Calculates a bounding rectangle for output thumbnail.
    QRect calculateContentRect() const
    {
        QRect result;
        bool first = true;
        for (const ItemPtr& item : items)
        {
            auto rect = item->outRect;
            if (rect.isEmpty())
                continue;

            if (first)
            {
                result = rect;
                first = false;
            }
            else
            {
                result = result.united(rect);
            }
        }
        return result;
    }

    // Resets internal state
    void reset()
    {
        items.clear();
        outputImage = QImage();
        status = Qn::ThumbnailStatus::Invalid;
        numLoading = 0;
    }

    int numLoading = 0;

    // We draw all items to this image.
    QImage outputImage;
    // We cut a part of outputImage and send it to clients.
    QImage outputRegion;
    // Output status of thumbnail loader.
    Qn::ThumbnailStatus status = Qn::ThumbnailStatus::Invalid;
    // Items to be loaded.
    QList<ItemPtr> items;

    nx::api::ImageRequest::RoundMethod roundMethod = nx::api::ImageRequest::RoundMethod::precise;
    bool tolerant = false; //< See nx::api::ImageRequest::tolerant.

    // We need this widgets to draw special states, like 'NoData'.
    QScopedPointer<AutoscaledPlainText> noDataWidget;
    QScopedPointer<QWidget> offlineWidget;
    QScopedPointer<QWidget> noExportPermissionWidget;
    QScopedPointer<AutoscaledPlainText> nonCameraWidget;
};

LayoutThumbnailLoader::LayoutThumbnailLoader(
    const QnLayoutResourcePtr& layout,
    const QSize& maximumSize,
    qint64 msecSinceEpoch,
    bool skipExportPermissionCheck,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(this, layout, maximumSize, msecSinceEpoch, skipExportPermissionCheck))
{
}

LayoutThumbnailLoader::~LayoutThumbnailLoader()
{
}

QnLayoutResourcePtr LayoutThumbnailLoader::layout() const
{
    return d->layout;
}

void LayoutThumbnailLoader::setWatermark(const nx::core::Watermark& watermark)
{
    d->watermark = watermark;
}

QImage LayoutThumbnailLoader::image() const
{
    switch (d->status)
    {
        case Qn::ThumbnailStatus::NoData:
        case Qn::ThumbnailStatus::Invalid:
        case Qn::ThumbnailStatus::Refreshing:
            return QImage();
        default:
            return d->outputRegion;
    }
}

QSize LayoutThumbnailLoader::sizeHint() const
{
    return d->outputRegion.size();
}

Qn::ThumbnailStatus LayoutThumbnailLoader::status() const
{
    return d->status;
}

QColor LayoutThumbnailLoader::itemBackgroundColor() const
{
    return d->noDataWidget->palette().color(QPalette::Window);
}

void LayoutThumbnailLoader::setItemBackgroundColor(const QColor& value)
{
    setPaletteColor(d->noDataWidget.data(), QPalette::Window, value);
}

void LayoutThumbnailLoader::setRequestRoundMethod(nx::api::ImageRequest::RoundMethod roundMethod)
{
    d->roundMethod = roundMethod;
}

void LayoutThumbnailLoader::setTolerant(bool value)
{
    d->tolerant = value;
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

    d->reset();

    const bool hasBackground = !d->layout->backgroundImageFilename().isEmpty();
    const auto backgroundRect = d->layout->backgroundRect();

    const common::LayoutItemDataMap& layoutItems = d->layout->getItems();

    std::vector<std::pair<common::LayoutItemData, QnResourcePtr>> validItems;
    validItems.reserve(layoutItems.size());

    // This is initial bounding box. This calculation uses cell sizes, instead of a pixels.
    // Right now we do not know actual pixel size of layout items.
    // Pixel-accurate calculations will be done in Private::finalizeOutputImage.
    for (auto iter = layoutItems.constBegin(); iter != layoutItems.constEnd(); ++iter)
    {
        const common::LayoutItemData& item = iter.value();
        const auto& itemRect = item.combinedGeometry;
        if (!itemRect.isValid()) // TODO: #sivanov Some items can be not placed yet.
            continue;

        const auto resource = getResourceByDescriptor(iter->resource);
        if (!resource)
            continue;

        validItems.emplace_back(item, resource);
    }

    // This is initial bounding box. This calculation uses cell sizes, instead of a pixels.
    // Right now we do not know actual pixel size of layout items.
    // Pixel-accurate calculations will be done in Private::finalizeOutputImage.
    QRectF bounding;
    if (!d->layout->fixedSize().isEmpty())
        bounding = d->layout->backgroundRect(d->layout->fixedSize());

    for (const auto& [item, resource]: validItems)
        bounding = bounding.united(item.combinedGeometry);

    if (hasBackground)
        bounding = bounding.united(backgroundRect);

    if (bounding.isEmpty())
    {
        d->status = Qn::ThumbnailStatus::NoData;
        // Dat proper event chain.
        emit sizeHintChanged(QSize());
        emit statusChanged(d->status);
        return;
    }

    // Handle negative spacing for exported layouts.
    const qreal spacing = std::max(0.0, d->layout->cellSpacing());

    const qreal cellAspectRatio = d->layout->hasCellAspectRatio()
        ? d->layout->cellAspectRatio()
        : QnLayoutResource::kDefaultCellAspectRatio;

    qreal xscale, yscale;
    const qreal sourceAr = cellAspectRatio * bounding.width() / bounding.height();

    const qreal targetAr = Geometry::aspectRatio(d->maximumSize);
    if (sourceAr > targetAr)
    {
        xscale = ceilf(d->maximumSize.width() / bounding.width());
        yscale = ceilf(xscale / cellAspectRatio);
    }
    else
    {
        yscale = ceilf(d->maximumSize.height() / bounding.height());
        xscale = yscale * cellAspectRatio;
    }

    // Pretty name for debug output
    d->layoutName = d->layout->getName();
    if (d->layoutName.isEmpty())
        d->layoutName = lit("<Layout>");

    QSize outputSize(bounding.width() * xscale, bounding.height() * yscale);
    d->outputImage = QImage(outputSize, QImage::Format_ARGB32_Premultiplied);

    // Distinct color for debugging.
    if (ini().debugThumbnailProviders)
        d->outputImage.fill(Qt::darkBlue);
    else
        d->outputImage.fill(Qt::transparent);

    d->status = Qn::ThumbnailStatus::Loading;

    emit sizeHintChanged(d->outputImage.size());
    emit statusChanged(d->status);

    if (hasBackground)
    {
        // Ensure that loader is not forgotten before it produces any result.
        Private::ItemPtr item = d->newItem();

        item->outCellRect = QRectF(
            (backgroundRect.left() - bounding.left()) * xscale,
            (backgroundRect.top() - bounding.top()) * yscale,
            backgroundRect.width() * xscale,
            backgroundRect.height() * yscale);

        item->name = lit("Background");

        item->provider.reset(
            new LayoutBackgroundImageProvider(d->layout, kBackgroundPreviewSize));

        // We connect only to statusChanged event.
        // We expect that provider sends signals in a proper order
        // and it already has generated image.
        connect(item->provider.data(), &ImageProvider::statusChanged, this,
            [this, item](Qn::ThumbnailStatus status)
            {
                if (status == Qn::ThumbnailStatus::Loaded)
                {
                    d->drawBackground(item, item->provider->image());
                    d->notifyLoaderIsComplete();
                }
            });

        item->provider->loadAsync();
    }

    for (const auto& [data, resource]: validItems)
    {
        Private::ItemPtr thumbnailItem = d->newItem();

        thumbnailItem->resource = resource;
        thumbnailItem->rotation = data.rotation;
        thumbnailItem->name = resource->getName();

        // Cell bounds.
        const auto& cellRect = data.combinedGeometry;
        thumbnailItem->outCellRect = QRectF(
            (cellRect.left() - bounding.left() + spacing * 0.5) * xscale,
            (cellRect.top() - bounding.top() + spacing * 0.5) * yscale,
            (cellRect.width() - spacing) * xscale,
            (cellRect.height() - spacing) * yscale);

        // Deal with zoom.
        QSize thumbnailSize(0, thumbnailItem->outCellRect.height());
        const auto zoomRect = data.zoomRect;
        if (!zoomRect.isEmpty())
            thumbnailSize /= zoomRect.height();

        using namespace std::chrono;

        // Fill in request for the thumbnail.
        // We will warp this image later.
        nx::api::ResourceImageRequest request;
        request.resource = thumbnailItem->resource;
        request.timestampMs = milliseconds(d->msecSinceEpoch);
        // Here we imitate old FfmpegImageProvider behavior where it ignored position
        // and always returned middle screenshot.
        if (thumbnailItem->resource.dynamicCast<QnAviResource>())
            request.timestampMs = nx::api::ImageRequest::kLatestThumbnail;

        request.size = thumbnailSize;
        request.rotation = 0;
        // server still should provide most recent frame when we request request.msecSinceEpoch = -1
        request.roundMethod = d->roundMethod;
        request.tolerant = d->tolerant;

        ImageProvider* provider = nullptr;
        if (d->watermark.visible())
        {
            auto baseProvider = new ResourceThumbnailProvider(request, this);
            auto finalProvider = new WatermarkProxyProvider(baseProvider, baseProvider);
            finalProvider->setWatermark(d->watermark);
            provider = finalProvider;
        }
        else
            provider = new ResourceThumbnailProvider(request);

        thumbnailItem->provider.reset(provider);

        const auto handleStatusChange =
            [this, thumbnailItem, zoomRect](Qn::ThumbnailStatus status)
            {
                QnAspectRatio sourceAr(thumbnailItem->provider->sizeHint());
                if (status == Qn::ThumbnailStatus::Loaded)
                {
                    const auto sourceAr =
                        Geometry::aspectRatio(thumbnailItem->provider->sizeHint());
                    auto aspectRatio = zoomRect.isNull()
                        ? sourceAr
                        : sourceAr * Geometry::aspectRatio(zoomRect);
                    d->drawTile(thumbnailItem, aspectRatio, zoomRect);
                }
                // TODO: Why do we use different types for aspect ratio
                // in drawTile and updateTileStatus ?
                d->updateTileStatus(status, sourceAr, thumbnailItem);
            };

        if (d->skipExportPermissionCheck || hasExportPermission(resource))
        {
            // We connect only to statusChanged event.
            // We expect that provider sends signals in a proper order
            // and it already has generated image.
            connect(thumbnailItem->provider.data(), &ImageProvider::statusChanged, this,
                handleStatusChange);

            thumbnailItem->provider->loadAsync();
        }
        else
        {
            QMetaObject::invokeMethod(this,
                [handleStatusChange]()
                {
                    handleStatusChange(Qn::ThumbnailStatus::NoData);
                },
                Qt::QueuedConnection);
        }
    }

    if (d->numLoading == 0)
    {
        // If there's nothing to load.
        // TODO: #dkargin I guess we should draw NoData here.
        d->status = Qn::ThumbnailStatus::NoData;
        emit statusChanged(d->status);
    }
}

} // namespace nx::vms::client::desktop
