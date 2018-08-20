#include "watermark_images.h"

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
const int kWatermarkFontSize = 70;
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
    QFontMetrics metrics(font);
    int width = metrics.width(watermark.text);
    if (width <= 0)
        return pixmap; //< Just in case m_text is still non-printable.

    int xCount = (int) (1 + watermark.settings.frequency * 9.99); //< xCount = 1..10 .

    // #sandreenko - this will be moved out in a few commits.
    // Fix font size so that text will fit xCount times horizontally.
    // We want text occupy 2/3 size of each rectangle (voluntary).
    // while (width * xCount < (2 * pixmap.width()) / 3)
    // {
    //     font.setPixelSize(font.pixelSize() + 1);
    //     width = QFontMetrics(font).width(text);
    // }
    // while ((width * xCount > (2 * pixmap.width()) / 3) && font.pixelSize() > 2)
    // {
    //     font.setPixelSize(font.pixelSize() - 1);
    //     width = QFontMetrics(font).width(text);
    // }
    font.setPixelSize(kWatermarkFontSize);

    int yCount = std::max(1, (2 * pixmap.height()) / (3 * QFontMetrics(font).height()));

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
