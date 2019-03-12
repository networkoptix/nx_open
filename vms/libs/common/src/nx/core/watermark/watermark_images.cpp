#include "watermark_images.h"

#include <cmath>
#include <array>
#include <algorithm>

#include <QtCore/QMap>
#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtGui/QFont>
#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>

#include <nx/core/watermark/watermark.h>
#include <utils/graphics/drop_shadow_filter.h>

using nx::core::Watermark;

namespace {

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
QMap<double, QPixmap> watermarkImages;

void checkWatermarkChange(const Watermark& watermark)
{
    if (watermark != cachedWatermark)
    {
        cachedWatermark = watermark;
        watermarkImages.clear(); //< Drop obsolete images.
    }
}

QPixmap getCachedPixmapByAspectRatio(double aspectRatio)
{
    // Try to find any value in the interval
    // (aspectRatio / fuzzyEqualRatio, aspectRatio * fuzzyEqualRatio).
    const auto item = watermarkImages.lowerBound(aspectRatio / kFuzzyEqualRatio);
    if (item == watermarkImages.end())
        return QPixmap();

    if (item.key() < aspectRatio * kFuzzyEqualRatio)
        return item.value();

    return QPixmap();
}


QPixmap createAndCacheWatermarkImage(const Watermark& watermark, QSize size)
{
    double aspectRatio = ((double) size.width()) / size.height();
    //^ Ask Misha Sh. about this weird code style ^.

    bool predefinedAspect = false;
    // Setup predefined sizes for a few well-known aspect ratios.
    for (int i = 0; i < predefinedSizes.size(); i++)
    {
        if ((fabs(aspectRatio / predefinedSizes[i].first - 1.0) < kFuzzyEqualDiff))
        {
            aspectRatio = predefinedSizes[i].first;
            size = predefinedSizes[i].second;
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

    // Create new watermark pixmap.
    auto pixmap = nx::core::createWatermarkImage(watermark, size);

    // And push it into cache.
    watermarkImages[aspectRatio] = pixmap;

    return pixmap;
}

QPixmap createWatermarkTile(const Watermark& watermark, const QSize& size, const QFont& font)
{
    QImage tile(size, QImage::Format_ARGB32_Premultiplied);
    tile.fill(Qt::transparent);

    QPainter painter(&tile);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setPen(kWatermarkColor);
    painter.setFont(font);
    painter.drawText(0, 0, size.width(), size.height(), Qt::AlignCenter, watermark.text);
    painter.end(); //< or we may crush in painter destructor later.

    nx::utils::graphics::DropShadowFilter().filterImage(tile); //< Add shadow.

    QPixmap finalTile(tile.size());
    finalTile.fill(Qt::transparent);
    QPainter painter1(&finalTile);
    painter1.setOpacity(watermark.settings.opacity); //< Use opacity to make watermark semi-transparent.
    painter1.drawImage(QPoint(0, 0), tile);

    return finalTile;
}

} // namespace

QPixmap nx::core::createWatermarkImage(const Watermark& watermark, const QSize& size)
{
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);

    if (watermark.text.isEmpty())
        return pixmap;

    QFont font;
    int fontSize = kWatermarkFontSize;
    font.setPixelSize(fontSize);
    QFontMetrics metrics(font);
    int width = metrics.width(watermark.text);
    if (width <= 0)
        return pixmap; //< Just in case m_text is still non-printable.

    if ((width * 3) / 2 > size.width()) //< Decrease font if inscription is too wide.
    {
        fontSize = (kWatermarkFontSize * size.width() * 2) / (3 * width);
        if (fontSize < 5) //< Watermark will not be readable.
            return pixmap;

        font.setPixelSize(fontSize);
    }

    QSize inscriptionSize = QFontMetrics(font).size(0, watermark.text);
    const int minTileWidth = (3 * inscriptionSize.width()) / 2; //< Inscription takes max 2/3 tile width.
    const int minTileHeight = 2 * inscriptionSize.height(); //< Inscription takes max 1/2 tile height.

    int xCount = (int) (1 + watermark.settings.frequency * 9.99); //< xCount = 1..10 .
    int yCount = xCount;
    xCount = std::max(1, std::min(pixmap.width() / minTileWidth, xCount));
    yCount = std::max(1, std::min(pixmap.height() / minTileHeight, yCount));

    width = pixmap.width() / xCount;
    const int height = pixmap.height() / yCount;

    QPixmap tile = createWatermarkTile(watermark, QSize(width, height), font);

    QPainter painter(&pixmap);
    for (int x = 0; x < xCount; x++)
    {
        for (int y = 0; y < yCount; y++)
        {
            painter.drawPixmap((int)((x * pixmap.width()) / xCount),
                (int)((y * pixmap.height()) / yCount), tile);
        }
    }

    return pixmap;
}

QPixmap nx::core::retrieveWatermarkImage(const Watermark& watermark, const QSize& size)
{
    // Check to avoid future possible division by zero.
    if (size.isEmpty())
        return QPixmap();

    checkWatermarkChange(watermark);

    QPixmap pixmap = getCachedPixmapByAspectRatio(((double) size.width()) / size.height());

    if (!pixmap.isNull())
        return pixmap; //< Retrieved from cache successfully.

    return createAndCacheWatermarkImage(watermark, size);
}
