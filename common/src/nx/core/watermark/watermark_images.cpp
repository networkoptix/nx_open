#include "watermark_images.h"

#include <cmath>
#include <algorithm>

#include <QtCore/QMap>
#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtGui/QFont>
#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>

#include <nx/core/watermark/watermark.h>

using nx::core::Watermark;

namespace {

const QColor kWatermarkColor = QColor(Qt::white);
const int kWatermarkFontSize = 42;
constexpr double fuzzyEqualDiff = 0.02;
constexpr double fuzzyEqualRatio = 1.02;

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
    auto item = watermarkImages.lowerBound(aspectRatio / fuzzyEqualRatio);
    if (item == watermarkImages.end())
        return QPixmap();

    if (item.key() < aspectRatio * fuzzyEqualRatio)
        return item.value();

    return QPixmap();
}


QPixmap createAndCacheWatermarkImage(const Watermark& watermark, QSize size)
{
    double aspectRatio = ((double) size.width()) / size.height();
    //^ Ask Misha Sh. about this weird code style ^.

    // Setup predefined sizes for a few well-known aspect ratios.
    // These values are arbitrary, but reasonable.
    if (fabs(aspectRatio / (16.0 / 9.0) - 1.0) < fuzzyEqualDiff)
    {
        size = QSize(1920, 1080);
        aspectRatio = 16.0 / 9.0;
    }
    if (fabs(aspectRatio / (4.0 / 3.0) - 1.0) < fuzzyEqualDiff)
    {
        size = QSize(1600, 1200);
        aspectRatio = 4.0 / 3.0;

    }
    if (fabs(aspectRatio - 1) < fuzzyEqualDiff)
    {
        size = QSize(1200, 1200);
        aspectRatio = 1;
    }

    // Create new watermark pixmap.
    auto pixmap = nx::core::createWatermarkImage(watermark, size);

    // And push it into cache.
    watermarkImages[aspectRatio] = pixmap;

    return pixmap;
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

    if (width * 2 > size.width()) //< Decrease font if inscription is too wide.
    {
        fontSize = (kWatermarkFontSize * size.width()) / (2 * width);
        if (fontSize < 5) //< Watermark will not be readable.
            return pixmap;

        font.setPixelSize(fontSize);
    }

    QSize inscriptionSize = QFontMetrics(font).size(0, watermark.text);
    int minTileWidth = 2 * inscriptionSize.width(); //
    int minTileHeight = 2 * inscriptionSize.height();

    int xCount = (int) (1 + watermark.settings.frequency * 9.99); //< xCount = 1..10 .
    int yCount = xCount;
    xCount = std::max(1, std::min(pixmap.width() / minTileWidth, xCount));
    yCount = std::max(1, std::min(pixmap.height() / minTileHeight, yCount));

    QPainter painter(&pixmap);
    QColor color = kWatermarkColor;
    color.setAlphaF(watermark.settings.opacity);
    painter.setPen(color);
    painter.setFont(font);

    width = pixmap.width() / xCount;
    int height = pixmap.height() / yCount;
    for (int x = 0; x < xCount; x++)
    {
        for (int y = 0; y < yCount; y++)
        {
            painter.drawText((int)((x * pixmap.width()) / xCount),
                (int) ((y * pixmap.height()) / yCount),
                width,
                height,
                Qt::AlignCenter,
                watermark.text);
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
