// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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

    if (rect.isEmpty()) //< Just a double-check to avoid division by zero later.
        return;

    const auto size = rect.toRect().size();
    if (!m_pixmap || m_size != size)
        updateWatermarkImage(size); //< First-time call, need to get image.

    // We are scaling our bitmap to the output rectangle. Probably there is better solution.
    painter->drawPixmap(rect, m_pixmap, m_pixmap.rect());
}

void WatermarkPainter::setWatermark(nx::core::Watermark watermark)
{
    m_watermark = watermark;
    m_watermark.text = watermark.text.trimmed(); //< Whitespace is stripped, so that the text is empty or printable.
    m_pixmap = QPixmap(); //< Destroy current image because now we need a new one.
}

const nx::core::Watermark& WatermarkPainter::watermark() const
{
    return m_watermark;
}

void WatermarkPainter::updateWatermarkImage(const QSize& size)
{
    m_pixmap = nx::core::retrieveWatermarkImage(m_watermark, size);
    m_size = size;
}

} // namespace nx::vms::client::desktop
