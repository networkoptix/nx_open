#include "sharp_pixmap_painting.h"

#include <utils/common/scoped_painter_rollback.h>

namespace {
    const qreal kSharpnessEps = 0.0001;
} // namespace

QTransform sharpTransform(const QTransform& transform, bool* corrected)
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

void paintSharp(QPainter* painter, std::function<void(QPainter*)> paint)
{
    NX_EXPECT(paint);
    if (!paint)
        return;

    bool corrected = false;
    const QTransform roundedTransform = sharpTransform(painter->transform(), &corrected);
    if (corrected)
    {
        const QnScopedPainterTransformRollback rollback(painter, roundedTransform);
        paint(painter);
    }
    else
    {
        paint(painter);
    }
}

void paintPixmapSharp(QPainter* painter, const QPixmap& pixmap, const QPointF& position)
{
    const auto targetRect = QRectF(position, pixmap.size() / pixmap.devicePixelRatio());
    paintPixmapSharp(painter, pixmap, targetRect);
}

void paintPixmapSharp(
    QPainter* painter,
    const QPixmap& pixmap,
    const QRectF& rect,
    const QRect& sourceRect)
{
    NX_ASSERT(painter, Q_FUNC_INFO, "Painter must exist here");
    if (!painter || pixmap.isNull())
        return;

    const auto srcRect = sourceRect.isValid() ? sourceRect : pixmap.rect();

    bool corrected = false;
    const QTransform roundedTransform = sharpTransform(painter->transform(), &corrected);
    const QPointF roundedPosition(qRound(rect.x()), qRound(rect.y()));
    const auto targetRect = QRectF(roundedPosition, rect.size());

    if (corrected)
    {
        const QnScopedPainterTransformRollback rollback(painter, roundedTransform);
        painter->drawPixmap(targetRect, pixmap, srcRect);
    }
    else
    {
        painter->drawPixmap(targetRect, pixmap, srcRect);
    }
}
