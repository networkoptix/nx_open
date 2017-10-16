#include "text_pixmap_painter.h"

#include <nx/client/core/utils/geometry.h>

#include "text_pixmap_cache.h"

void QnTextPixmapPainter::drawText(
    QPainter* painter,
    const QRectF& rect,
    Qt::Alignment alignment,
    const QString& text,
    const QFont& font,
    const QColor& color)
{
    using nx::client::core::utils::Geometry;

    QPixmap pixmap = QnTextPixmapCache::instance()->pixmap(text, font, color);

    painter->drawPixmap(
        Geometry::expanded(
            Geometry::aspectRatio(pixmap.rect()),
            rect,
            Qt::KeepAspectRatio,
            alignment
        ),
        pixmap,
        pixmap.rect()
    );
}

void QnTextPixmapPainter::drawText(
    QPainter* painter,
    const QRectF& rect,
    Qt::Alignment alignment,
    const QString& text)
{
    drawText(painter, rect, alignment, text, painter->font(), painter->pen().color());
}
