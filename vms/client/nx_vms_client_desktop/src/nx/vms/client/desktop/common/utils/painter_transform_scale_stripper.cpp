// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "painter_transform_scale_stripper.h"

#include <cmath>

#include <nx/utils/log/assert.h>
#include <nx/utils/math/fuzzy.h>

namespace nx::vms::client::desktop {

namespace {

QTransform filteredTransform(const QTransform& transform)
{
    // When paint events calculate transformation for QPainter they obviously have some rounding
    // errors, and may produce slightly varying values in the transformation matrix. Practically
    // there are values very close to zero (smaller than 1e-14). Despite this fact, those values
    // do affect the resulting coordinates which may be 1 screen pixel more or less, what causes
    // annoying jitter of the displaying picture. To fix that, we round translation values of the
    // transformation matrix, which are considered very close to their rounded value.

    const auto filter =
        [](qreal x)
        {
            const qreal rounded = std::round(x);
            return qFuzzyEquals(x, rounded) ? rounded : x;
        };

    return QTransform(
        transform.m11(), transform.m12(), transform.m13(),
        transform.m21(), transform.m22(), transform.m23(),
        filter(transform.m31()), filter(transform.m32()), transform.m33());
}

} // namespace

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
        m_transform = filteredTransform(m_originalTransform);
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
            m_transform = filteredTransform(QTransform::fromScale(sx, sy) * translation);
            return;
        }
    }

    // 3. The most complicated case: non-conformal mapping.
    NX_ASSERT(false, "Non-conformal mapping is not supported.");
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
    return m_alignable ? mapped.toRect() : mapped;
}

QPointF PainterTransformScaleStripper::mapPoint(const QPointF& point) const
{
    return m_transform.map(point);
}

const QTransform& PainterTransformScaleStripper::transform() const
{
    return m_transform;
}

} // namespace nx::vms::client::desktop
