#ifndef QN_TEXT_PIXMAP_PAINTER_H
#define QN_TEXT_PIXMAP_PAINTER_H

#include <QtGui/QPainter>

class QnTextPixmapPainter {
public:
    static void drawText(QPainter *painter, const QRectF &rect, Qt::Alignment alignment, const QString &text, const QFont &font, const QColor &color);
    static void drawText(QPainter *painter, const QRectF &rect, Qt::Alignment alignment, const QString &text);
};


#endif // QN_TEXT_PIXMAP_PAINTER_H
