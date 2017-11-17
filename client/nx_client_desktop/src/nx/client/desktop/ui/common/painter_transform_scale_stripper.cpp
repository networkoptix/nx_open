#include "painter_transform_scale_stripper.h"

#include <cmath>

#include <nx/utils/log/assert.h>
#include <nx/utils/math/fuzzy.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

PainterTransformScaleStripper::PainterTransformScaleStripper(QPainter* painter):
    m_painter(painter),
    m_originalTransform(painter->transform()),
    m_type(m_originalTransform.type())
{
    // 1. The simplest case: only translate & scale.
    if (m_type <= QTransform::TxScale
        && m_originalTransform.m11() > 0
        && m_originalTransform.m22() > 0)
    {
        m_transform = m_originalTransform;
        painter->setTransform(QTransform());
        m_alignable = true;
        return;
    }

    // 2. More complicated case: with possible rotation.
    if (m_type <= QTransform::TxRotate)
    {
        const auto sx = hypot(m_originalTransform.m11(), m_originalTransform.m12());
        const auto sy = hypot(m_originalTransform.m21(), m_originalTransform.m22());

        if (qFuzzyEquals(qAbs(sx), qAbs(sy))) //< Uniform scale, no shear; possible mirroring.
        {
            NX_ASSERT(!qFuzzyIsNull(sx));
            if (qFuzzyIsNull(sx)) //< Just in case.
                return;

            m_alignable = qFuzzyIsNull(m_originalTransform.m11())
                || qFuzzyIsNull(m_originalTransform.m12());

            // Strip transformation of scale and translation.
            const QTransform rotation(
                m_originalTransform.m11() / sx, m_originalTransform.m12() / sx,
                m_originalTransform.m21() / sy, m_originalTransform.m22() / sy,
                0.0, 0.0);

            const QTransform translation = rotation
                * QTransform::fromTranslate(m_originalTransform.dx(), m_originalTransform.dy())
                * rotation.transposed(); //< For rotation matrix transposition === inversion.

            painter->setTransform(rotation);
            m_transform = QTransform::fromScale(sx, sy) * translation;
            return;
        }
    }

    // 3. The most complicated case: non-conformal mapping.
    NX_ASSERT(false, Q_FUNC_INFO, "Non-conformal mapping is not supported.");
}

PainterTransformScaleStripper::~PainterTransformScaleStripper()
{
    m_painter->setTransform(m_originalTransform);
}

QTransform::TransformationType PainterTransformScaleStripper::type() const
{
    return m_type;
}

QRectF PainterTransformScaleStripper::mapRect(const QRectF& rect) const
{
    const auto mapped = m_transform.mapRect(rect);
    return m_alignable ? mapped.toAlignedRect() : mapped;
}

QPointF PainterTransformScaleStripper::mapPoint(const QPointF& point) const
{
    return m_transform.map(point);
}

const QTransform& PainterTransformScaleStripper::transform() const
{
    return m_transform;
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
