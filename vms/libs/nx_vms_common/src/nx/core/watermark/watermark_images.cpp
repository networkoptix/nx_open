// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "watermark_images.h"

#include <cmath>
#include <array>
#include <algorithm>

#include <QtCore/QMap>
#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtGui/QFont>
#include <QtGui/QPainter>

#include <nx/core/watermark/watermark.h>
#include <nx/utils/log/log.h>
#include <nx/utils/text_renderer.h>
#include <utils/graphics/drop_shadow_filter.h>

using nx::core::Watermark;

namespace {

static const QString kFontFamily = "Roboto-Regular";

const QColor kWatermarkColor = QColor(Qt::white);
const int kWatermarkFontSize = 84; //< Pure magic. It was 42 * 2 before. No more memes.
constexpr double kFuzzyEqualDiff = 0.02;
constexpr double kFuzzyEqualRatio = 1.02;

// Predefined watermark image sizes for typical aspect ratios, pair<aspect ratio, image size>.
// These values are arbitrary, but reasonable.
const std::array<std::pair<double, QSize>, 5> predefinedSizes = {{
    {16.0 / 9.0, QSize(1920, 1080)},
    {4.0 / 3.0,  QSize(1600, 1200)},
    {1.0,        QSize(1200, 1200)},
    {3.0 / 4.0,  QSize(1200, 1600)},
    {9.0 / 16.0, QSize(1080, 1920)}}};

Watermark cachedWatermark;
QMap<double, QImage> watermarkImages;

void checkWatermarkChange(const Watermark& watermark)
{
    if (watermark != cachedWatermark)
    {
        cachedWatermark = watermark;
        watermarkImages.clear(); //< Drop obsolete images.
    }
}

QImage getCachedimageByAspectRatio(double aspectRatio)
{
    // Try to find any value in the interval
    // (aspectRatio / fuzzyEqualRatio, aspectRatio * fuzzyEqualRatio).
    const auto item = watermarkImages.lowerBound(aspectRatio / kFuzzyEqualRatio);
    if (item == watermarkImages.end())
        return QImage();

    if (item.key() < aspectRatio * kFuzzyEqualRatio)
        return item.value();

    return QImage();
}

QImage createAndCacheWatermarkImage(const Watermark& watermark, QSize size)
{
    double aspectRatio = ((double) size.width()) / size.height();
    //^ Ask Misha Sh. about this weird code style ^.

    bool predefinedAspect = false;
    // Setup predefined sizes for a few well-known aspect ratios.
    for (const auto& [predefinedRatio, predefinedSize]: predefinedSizes)
    {
        if ((fabs(aspectRatio / predefinedRatio - 1.0) < kFuzzyEqualDiff))
        {
            aspectRatio = predefinedRatio;
            size = predefinedSize;
            predefinedAspect = true;
            break;
        }
    }

    // Sometimes `size` uses logical coordinates that are quite big, ~10000.
    // We will limit such bitmaps to 1600 px (magical) on bigger side.
    constexpr int maxSize = 1600;
    if (!predefinedAspect && (size.width() > maxSize || size.height() > maxSize))
    {
        size = size.width() > size.height()
            ? QSize(maxSize, size.height() * maxSize / size.width())
            : QSize(size.width() * maxSize / size.height(), maxSize);
    }

    // We also limit biggest dimension to be at least 1000. This is needed mostly
    // for small previews during export.
    constexpr int minSize = 1200;
    if (!predefinedAspect && (size.width() < minSize && size.height() < minSize))
    {
        size = size.width() > size.height()
            ? QSize(minSize, size.height() * minSize / size.width())
            : QSize(size.width() * minSize / size.height(), minSize);
    }

    // Create new watermark image.
    auto image = nx::core::createWatermarkImage(watermark, size);

    // And push it into cache.
    watermarkImages[aspectRatio] = image;

    return image;
}

QSize getTextSize(const QFont& font, const QString& text)
{
    std::unique_ptr<nx::utils::AbstractTextRenderer> renderer =
        nx::utils::TextRendererFactory::create();
    if (!renderer)
        return QSize();
    renderer->fontFamily = kFontFamily;
    renderer->fontPixelSize = font.pixelSize();
    return renderer->textSize(text);
}

void drawTextToImage(QImage& image, const QSize& size, const QFont& font, const QString& text)
{
    std::unique_ptr<nx::utils::AbstractTextRenderer> renderer =
        nx::utils::TextRendererFactory::create();
    if (!renderer)
        return;
    renderer->fontFamily = kFontFamily;
    renderer->fontPixelSize = font.pixelSize();
    renderer->color = kWatermarkColor;
    renderer->drawText(image, QRect(0, 0, size.width(), size.height()), text);
}

QImage createWatermarkTile(const Watermark& watermark, const QSize& size, const QFont& font)
{
    QImage tile(size, QImage::Format_ARGB32_Premultiplied);
    tile.fill(Qt::transparent);

    drawTextToImage(tile, size, font, watermark.text);

    nx::utils::graphics::DropShadowFilter().filterImage(tile); //< Add shadow.

    QImage finalTile(tile.size(), QImage::Format_ARGB32_Premultiplied);
    finalTile.fill(Qt::transparent);
    QPainter painter1(&finalTile);
    painter1.setOpacity(watermark.settings.opacity); //< Use opacity to make watermark semi-transparent.
    painter1.drawImage(QPoint(0, 0), tile);

    return finalTile;
}

} // namespace

QImage nx::core::createWatermarkImage(const Watermark& watermark, const QSize& size)
{
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    if (watermark.text.isEmpty())
        return image;

    QFont font;
    int fontSize = kWatermarkFontSize;
    font.setPixelSize(fontSize);
    int width = getTextSize(font, watermark.text).width();
    if (width <= 0)
        return image; //< Just in case m_text is still non-printable.

    if ((width * 3) / 2 > size.width()) //< Decrease font if inscription is too wide.
    {
        fontSize = (kWatermarkFontSize * size.width() * 2) / (3 * width);
        if (fontSize < 5) //< Watermark will not be readable.
            return image;

        font.setPixelSize(fontSize);
    }

    QSize inscriptionSize = getTextSize(font, watermark.text);
    const int minTileWidth = (3 * inscriptionSize.width()) / 2; //< Inscription takes max 2/3 tile width.
    const int minTileHeight = 2 * inscriptionSize.height(); //< Inscription takes max 1/2 tile height.

    int xCount = (int) (1 + watermark.settings.frequency * 9.99); //< xCount = 1..10 .
    int yCount = xCount;
    xCount = std::max(1, std::min(image.width() / minTileWidth, xCount));
    yCount = std::max(1, std::min(image.height() / minTileHeight, yCount));

    width = image.width() / xCount;
    const int height = image.height() / yCount;

    QImage tile = createWatermarkTile(watermark, QSize(width, height), font);

    QPainter painter(&image);
    for (int x = 0; x < xCount; x++)
    {
        for (int y = 0; y < yCount; y++)
        {
            painter.drawImage((int)((x * image.width()) / xCount),
                (int)((y * image.height()) / yCount), tile);
        }
    }

    return image;
}

QImage nx::core::retrieveWatermarkImage(const Watermark& watermark, const QSize& size)
{
    // Check to avoid future possible division by zero.
    if (size.isEmpty())
        return QImage();

    checkWatermarkChange(watermark);

    QImage image = getCachedimageByAspectRatio(((double) size.width()) / size.height());

    if (!image.isNull())
        return image; //< Retrieved from cache successfully.

    return createAndCacheWatermarkImage(watermark, size);
}
