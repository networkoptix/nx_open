#include "watermark_painter.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QFont>
#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>

#include <ini.h>

namespace nx {
namespace client {
namespace desktop {

namespace {
// Pixmap is scaled on the mediawidget.
const QSize kWatermarkSize = QSize(1920, 1080);
const QColor kWatermarkColor = QColor(Qt::white);
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
    m_pixmap.fill(Qt::transparent);

    if (!nx::client::desktop::ini().enableWatermark)
        return;

    if (m_text.isEmpty())
        return;

    QFont font;
    QFontMetrics metrics(font);
    int width = metrics.width(m_text);
    if (width <= 0)
        return; //< Just in case m_text is still non-printable.

    int xCount = (int)(1 + m_settings.frequency * 9.99); //< xCount = 1..10 .
    // Fix font size so that text will fit xCount times horizontally.
    // We want text occupy 2/3 size of each rectangle (voluntary).
    while (width * xCount < (2 * m_pixmap.width()) / 3)
    {
        font.setPixelSize(font.pixelSize() + 1);
        width = QFontMetrics(font).width(m_text);
    }
    while ((width * xCount > (2 * m_pixmap.width()) / 3) && font.pixelSize() > 2)
    {
        font.setPixelSize(font.pixelSize() - 1);
        width = QFontMetrics(font).width(m_text);
    }

    int yCount = std::max(1, (2 * m_pixmap.height()) / (3 * QFontMetrics(font).height()));

    QPainter painter(&m_pixmap);
    QColor color = kWatermarkColor;
    color.setAlphaF(m_settings.opacity);
    painter.setPen(color);
    painter.setFont(font);

    width = m_pixmap.width() / xCount;
    int height = m_pixmap.height() / yCount;
    for (int x = 0; x < xCount; x++)
    {
        for (int y = 0; y < yCount; y++)
        {
            painter.drawText((int)((x * m_pixmap.width()) / xCount),
                (int)((y * m_pixmap.height()) / yCount),
                width,
                height,
                Qt::AlignCenter,
                m_text);
        }
    }
}

} // namespace desktop
} // namespace client
} // namespace nx
