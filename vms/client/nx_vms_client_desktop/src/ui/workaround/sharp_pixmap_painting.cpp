#include "sharp_pixmap_painting.h"

#include <array>

#include <utils/common/scoped_painter_rollback.h>

#include <nx/utils/log/assert.h>

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

QMatrix4x4 sharpMatrix(const QMatrix4x4& matrix, bool* corrected)
{
    static constexpr std::array kZeroIndices{1, 2, 3, 4, 6, 7, 8, 9, 11, 14};
    static constexpr std::array kOneIndices{0, 5, 10, 15};
    const auto data = matrix.data();

    // Check for XY-only translation matrix, using lower precision than Qt.
    if (std::all_of(
            kZeroIndices.begin(),
            kZeroIndices.end(),
            [data](auto i) { return qAbs(data[i]) < kSharpnessEps; })
        && std::all_of(
            kOneIndices.begin(),
            kOneIndices.end(),
            [data](auto i) { return qAbs(data[i] - 1.0) < kSharpnessEps; }))
    {
        if (corrected)
            *corrected = true;

        QMatrix4x4 result; //< Start with identity matrix.

        // Round the original translation vector components to the nearest integers.
        result.data()[12] = std::round(data[12]); //< X translation.
        result.data()[13] = std::round(data[13]); //< Y translation.
        return result;
    }

    if (corrected)
        *corrected = false;

    return matrix;
}

void paintSharp(QPainter* painter, std::function<void(QPainter*)> paint)
{
    NX_ASSERT(paint);
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
    NX_ASSERT(painter, "Painter must exist here");
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
