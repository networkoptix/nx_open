#include "sharp_pixmap_painting.h"

#include <utils/common/scoped_painter_rollback.h>

namespace
{
    const qreal kSharpnessEps = 0.0001;
}

QTransform sharpTransform(const QTransform &transform, bool *corrected)
{
    /* Hand-rolled check for translation transform,
    * with lower precision than standard QTransform::type. */
    if (qAbs(transform.m11() - 1.0) < kSharpnessEps &&
        qAbs(transform.m12()) < kSharpnessEps &&
        qAbs(transform.m21()) < kSharpnessEps &&
        qAbs(transform.m22() - 1.0) < kSharpnessEps &&
        qAbs(transform.m13()) < kSharpnessEps &&
        qAbs(transform.m23()) < kSharpnessEps &&
        qAbs(transform.m33() - 1.0) < kSharpnessEps)
    {
        /* Instead of messing with QPainter::SmoothPixmapTransform,
        * we simply adjust the transformation. */
        if (corrected)
            *corrected = true;
        return QTransform(1.0, 0.0, 0.0, 0.0, 1.0, 0.0, qRound(transform.m31()), qRound(transform.m32()), 1.0);
    }

    if (corrected)
        *corrected = false;

    return transform;
}

void paintPixmapSharp( QPainter *painter, const QPixmap &pixmap, const QPointF &position ) {
    Q_ASSERT_X(painter, Q_FUNC_INFO, "Painter must exist here");
    if (!painter || pixmap.isNull())
        return;

    bool corrected = false;
    QTransform roundedTransform = sharpTransform(painter->transform(), &corrected);
    QPointF roundedPosition(qRound(position.x()), qRound(position.y()));

    if (corrected)
    {
        QnScopedPainterTransformRollback rollback(painter, roundedTransform);
        painter->drawPixmap(roundedPosition, pixmap);
    }
    else
    {
        painter->drawPixmap(roundedPosition, pixmap);
    }
}
