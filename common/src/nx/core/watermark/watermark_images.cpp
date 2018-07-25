#include "watermark_images.h"

#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtGui/QFont>
#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>

#include <nx/core/watermark/watermark.h>

namespace {
const QColor kWatermarkColor = QColor(Qt::white);
const int kWatermarkFontSize = 70;
} // namespace

QPixmap nx::core::getWatermarkImage(const Watermark & watermark, const QSize & size)
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

    int xCount = (int)(1 + watermark.settings.frequency * 9.99); //< xCount = 1..10 .

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
                (int)((y * pixmap.height()) / yCount),
                width,
                height,
                Qt::AlignCenter,
                watermark.text);
        }
    }
    return pixmap;
}
