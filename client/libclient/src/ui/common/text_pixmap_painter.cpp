#include "text_pixmap_painter.h"

#include "text_pixmap_cache.h"
#include "geometry.h"

namespace {
    const int maxPointSize = 64;
    const int maxPixelSize = 64;
}

void QnTextPixmapPainter::drawText(QPainter *painter, const QRectF &rect, Qt::Alignment alignment, const QString &text, const QFont &font, const QColor &color) {
    QPixmap pixmap = QnTextPixmapCache::instance()->pixmap(text, font, color);

    painter->drawPixmap(
        QnGeometry::expanded(
            QnGeometry::aspectRatio(pixmap.rect()),
            rect,
            Qt::KeepAspectRatio,
            alignment
        ),
        pixmap,
        pixmap.rect()
    );
}

void QnTextPixmapPainter::drawText(QPainter *painter, const QRectF &rect, Qt::Alignment alignment, const QString &text) {
    drawText(painter, rect, alignment, text, painter->font(), painter->pen().color());
}
