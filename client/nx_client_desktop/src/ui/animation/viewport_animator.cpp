#include "viewport_animator.h"
#include <cmath> /* For std::log, std::exp. */
#include <limits>
#include <QtWidgets/QGraphicsView>
#include <QtCore/QMargins>
#include <nx/client/core/utils/geometry.h>
#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>
#include <utils/math/linear_combination.h>
#include <utils/math/magnitude.h>
#include "viewport_geometry_accessor.h"

using nx::client::core::Geometry;

ViewportAnimator::ViewportAnimator(QObject *parent):
    base_type(parent),
    m_accessor(new ViewportGeometryAccessor()),
    m_adjustedTargetRectValid(false)
{
    setAccessor(m_accessor);
}

ViewportAnimator::~ViewportAnimator() {
    stop();
}

void ViewportAnimator::setView(QGraphicsView *view) {
    stop();

    setTargetObject(view);
}

QGraphicsView *ViewportAnimator::view() const {
    return checked_cast<QGraphicsView *>(targetObject());
}

void ViewportAnimator::moveTo(const QRectF &rect, bool animate)
{
    m_adjustedTargetRect = rect;
    setTargetValue(adjustedToReal(rect));

    if (animate)
    {
        start();
    }
    else
    {
        stop();
        updateCurrentValue(adjustedToReal(rect));
    }

    m_adjustedTargetRectValid = true;
}

QMargins ViewportAnimator::viewportMargins(Qn::MarginTypes marginTypes) const
{
    QMargins result;
    if (marginTypes.testFlag(Qn::PanelMargins))
        result += m_panelMargins;
    if (marginTypes.testFlag(Qn::LayoutMargins))
        result += m_layoutMargins;
    return result;
}

void ViewportAnimator::setViewportMargins(const QMargins& margins, Qn::MarginType marginType)
{
    QMargins* field = nullptr;
    switch (marginType)
    {
        case Qn::PanelMargins:
            field = &m_panelMargins;
            break;
        case Qn::LayoutMargins:
            field = &m_layoutMargins;
            break;
        default:
            NX_EXPECT(false);
            return;
    }

    if (margins == *field)
        return;

    *field = margins;

    if (m_adjustedTargetRectValid)
        setTargetRect(adjustedToReal(m_adjustedTargetRect));
}

Qn::MarginFlags ViewportAnimator::marginFlags() const {
    return m_marginFlags;
}

void ViewportAnimator::setMarginFlags(Qn::MarginFlags marginFlags) {
    if(marginFlags == m_marginFlags)
        return;

    m_marginFlags = marginFlags;

    if(m_adjustedTargetRectValid)
        setTargetRect(adjustedToReal(m_adjustedTargetRect));
}

int ViewportAnimator::estimatedDuration(const QVariant &from, const QVariant &to) const {
    qreal aspectRatio = Geometry::aspectRatio(view()->viewport()->rect());
    QRectF startRect = Geometry::expanded(aspectRatio, from.toRectF(), Qt::KeepAspectRatioByExpanding, Qt::AlignCenter);
    QRectF targetRect = Geometry::expanded(aspectRatio, to.toRectF(), Qt::KeepAspectRatioByExpanding, Qt::AlignCenter);

    return base_type::estimatedDuration(startRect, targetRect);
}

void ViewportAnimator::updateTargetValue(const QVariant &newTargetValue) {
    m_adjustedTargetRectValid = false;

    base_type::updateTargetValue(newTargetValue);
}

QRectF ViewportAnimator::targetRect() const {
    return targetValue().toRectF();
}

void ViewportAnimator::setTargetRect(const QRectF &rect) {
    setTargetValue(rect);
}

QRectF ViewportAnimator::adjustedToReal(const QRectF &adjustedRect) const {
    return adjustedToReal(view(), adjustedRect);
}

QRectF ViewportAnimator::realToAdjusted(const QGraphicsView *view, const QRectF &realRect) const {
    if(view == NULL)
        return realRect; /* Graphics view was destroyed, cannot adjust. */

    QMarginsF relativeMargins = Geometry::cwiseDiv(viewportMargins(), view->viewport()->size());

    /* Calculate size to match real aspect ratio. */
    QSizeF fixedRealSize = Geometry::expanded(Geometry::aspectRatio(view->viewport()->size()), realRect.size(), Qt::KeepAspectRatioByExpanding);

    /* Calculate resulting size. */
    QSizeF adjustedSize = fixedRealSize;
    if(m_marginFlags & Qn::MarginsAffectSize)
        adjustedSize = Geometry::cwiseMul(adjustedSize, QSizeF(1.0, 1.0) - Geometry::sizeDelta(relativeMargins));

    /* Calculate resulting position. */
    QPointF adjustedCenter = realRect.center();
    if(m_marginFlags & Qn::MarginsAffectPosition) {
        QRectF fixedRealRect = QRectF(adjustedCenter - Geometry::toPoint(fixedRealSize) / 2, fixedRealSize);

        adjustedCenter = Geometry::eroded(fixedRealRect, Geometry::cwiseMul(fixedRealRect.size(), relativeMargins)).center();

        /* Adjust so that adjusted rect lies inside the real one. */
        adjustedCenter.rx() = qBound(fixedRealRect.left() + adjustedSize.width()  / 2, adjustedCenter.x(), fixedRealRect.right()  - adjustedSize.width()  / 2);
        adjustedCenter.ry() = qBound(fixedRealRect.top()  + adjustedSize.height() / 2, adjustedCenter.y(), fixedRealRect.bottom() - adjustedSize.height() / 2);
    }

    return QRectF(adjustedCenter - Geometry::toPoint(adjustedSize) / 2, adjustedSize);
}

QRectF ViewportAnimator::adjustedToReal(const QGraphicsView *view, const QRectF &adjustedRect) const {
    if(view == NULL)
        return adjustedRect; /* Graphics view was destroyed, cannot adjust. */

    const auto margins = viewportMargins();

    QMarginsF relativeMargins = Geometry::cwiseDiv(margins, view->viewport()->size());

    /* Calculate margins that can be used for expanding adjusted to real rect.
     * Note that these margins are still positive, so SceneUtility::dilated should be used with them. */
    QMarginsF inverseRelativeMargins = Geometry::cwiseDiv(relativeMargins, QSize(1.0, 1.0) - Geometry::sizeDelta(relativeMargins));

    /* Calculate size to match adjusted aspect ratio. */
    QSizeF fixedAdjustedSize;
    {
        QSize adjustedViewportSize = view->viewport()->size();
        if(m_marginFlags & Qn::MarginsAffectSize)
            adjustedViewportSize = Geometry::eroded(adjustedViewportSize, margins);

        fixedAdjustedSize = Geometry::expanded(Geometry::aspectRatio(adjustedViewportSize), adjustedRect.size(), Qt::KeepAspectRatioByExpanding);
    }

    /* Calculate resulting size. */
    QSizeF realSize = fixedAdjustedSize;
    if(m_marginFlags & Qn::MarginsAffectSize)
        realSize = Geometry::cwiseMul(realSize, QSizeF(1.0, 1.0) + Geometry::sizeDelta(inverseRelativeMargins));

    /* Calculate resulting position. */
    QPointF realCenter = adjustedRect.center();
    if(m_marginFlags & Qn::MarginsAffectPosition) {
        QRectF fixedAdjustedRect = QRectF(realCenter - Geometry::toPoint(fixedAdjustedSize) / 2, fixedAdjustedSize);

        realCenter = Geometry::dilated(fixedAdjustedRect, Geometry::cwiseMul(fixedAdjustedRect.size(), inverseRelativeMargins)).center();

        /* Adjust so that adjusted rect lies inside the real one. */
        realCenter.rx() = qBound(adjustedRect.right()  - realSize.width()  / 2, realCenter.x(), adjustedRect.left() + realSize.width()  / 2);
        realCenter.ry() = qBound(adjustedRect.bottom() - realSize.height() / 2, realCenter.y(), adjustedRect.top()  + realSize.height() / 2);
    }

    return QRectF(realCenter - Geometry::toPoint(realSize) / 2, realSize);
}
