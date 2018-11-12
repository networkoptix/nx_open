#include "watermark_painter.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QFont>
#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>

#include <nx/core/watermark/watermark_images.h>

namespace nx::vms::client::desktop {

WatermarkPainter::WatermarkPainter()
{
}

void WatermarkPainter::drawWatermark(QPainter* painter, const QRectF& rect)
{
    if (!m_watermark.visible())
        return;

    if (rect.isEmpty()) //< Just a double-check to avoid division byzeroem later.
        return;

    if (!m_pixmap)
        updateWatermarkImage(rect.toRect().size()); //< First-time call, need to get image.

    // We are scaling our bitmap to the output rectangle. Probably there is better solution.
    painter->drawPixmap(rect, m_pixmap, m_pixmap.rect());
}

void WatermarkPainter::setWatermark(nx::core::Watermark watermark)
{
    m_watermark = watermark;
    m_watermark.text = watermark.text.trimmed(); //< Whitespace is stripped, so that the text is empty or printable.
    m_pixmap = QPixmap(); //< Destroy current image because now we need a new one.
}

void WatermarkPainter::updateWatermarkImage(const QSize& size)
{
    m_pixmap = nx::core::retrieveWatermarkImage(m_watermark, size);
}

} // namespace nx::vms::client::desktop
