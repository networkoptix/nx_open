#include "watermark_painter.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QFont>
#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>

#include <nx/core/transcoding/filters/watermark_images.h>

namespace nx {
namespace client {
namespace desktop {

namespace {
// Pixmap is scaled on the mediawidget.
const QSize kWatermarkSize = QSize(1920, 1080);
} // namespace

WatermarkPainter::WatermarkPainter()
{
    m_pixmap = QPixmap(kWatermarkSize);
    updateWatermark();
}

void WatermarkPainter::drawWatermark(QPainter* painter, const QRectF& rect)
{
    if (!m_settings.useWatermark)
        return;

    if (rect.isEmpty()) //< Just a double-check to avoid division byzeroem later.
        return;

    // We are scaling our bitmap to the output rectangle. Probably there is better solution.
    painter->drawPixmap(rect, m_pixmap, m_pixmap.rect());

    // #sandreenko this block or the line above will be removed; they represent different scaling approach.
    // // rect can be 10000x5000 or even more - so scale our pixmap preserving aspect ratio.
    // if (rect.width() * kWatermarkSize.height() > rect.height() * kWatermarkSize.width())
    //     painter->drawPixmap(rect, m_pixmap,
    //         QRectF(0, 0, m_pixmap.width(), (m_pixmap.width() * rect.height()) / rect.width()));
    // else
    //     painter->drawPixmap(rect, m_pixmap,
    //         QRectF(0, 0, (m_pixmap.height() * rect.width()) / rect.height(), m_pixmap.height()));
}

void WatermarkPainter::setWatermarkText(const QString& text)
{
    setWatermark(text, m_settings);
    updateWatermark();
}

void WatermarkPainter::setWatermarkSettings(const QnWatermarkSettings& settings)
{
    m_settings = settings;
    updateWatermark();
}

void WatermarkPainter::setWatermark(const QString& text, const QnWatermarkSettings& settings)
{
    m_text = text.trimmed(); //< Whitespace is stripped, so that the text is empty or printable.
    m_settings = settings;
    updateWatermark();
}

void WatermarkPainter::updateWatermark()
{
    m_pixmap = nx::core::transcoding::getWatermarkImage(m_settings, m_text, kWatermarkSize);
}

} // namespace desktop
} // namespace client
} // namespace nx
