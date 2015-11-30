#include "sharp_pixmap_painting.h"

namespace {
    const qreal sharpnessEps = 0.0001;
}

void paintPixmapSharp( QPainter *painter, const QPixmap &pixmap, const QPointF &position ) {
    Q_ASSERT_X(painter, Q_FUNC_INFO, "Painter must exist here");
    if (!painter || pixmap.isNull())
        return;

    QTransform transform = painter->transform();

    /* Hand-rolled check for translation transform,
    * with lower precision than standard QTransform::type. */
    if(
        qAbs(transform.m11() - 1.0) < sharpnessEps &&
        qAbs(transform.m12()) < sharpnessEps &&
        qAbs(transform.m21()) < sharpnessEps &&
        qAbs(transform.m22() - 1.0) < sharpnessEps &&
        qAbs(transform.m13()) < sharpnessEps &&
        qAbs(transform.m23()) < sharpnessEps &&
        qAbs(transform.m33() - 1.0) < sharpnessEps
        ) {
            /* Instead of messing with QPainter::SmoothPixmapTransform,
            * we simply adjust the transformation. */
            painter->setTransform(QTransform(1.0, 0.0, 0.0, 0.0, 1.0, 0.0, qRound(transform.m31() + position.x()), qRound(transform.m32() + position.y()), 1.0));
            painter->drawPixmap(QPointF(), pixmap);
            painter->setTransform(transform);
    } else {
        painter->drawPixmap(QPointF(), pixmap);
    }
}
