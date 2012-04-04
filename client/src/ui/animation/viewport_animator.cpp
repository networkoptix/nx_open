#include "viewport_animator.h"
#include <cmath> /* For std::log, std::exp. */
#include <limits>
#include <QGraphicsView>
#include <QMargins>
#include <ui/common/scene_utility.h>
#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>
#include <ui/common/linear_combination.h>
#include <ui/common/magnitude.h>
#include "viewport_geometry_accessor.h"

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

void ViewportAnimator::moveTo(const QRectF &rect, bool animate) {
    m_adjustedTargetRect = rect;
    
    if(animate) {
        setTargetValue(adjustedToReal(rect));
        start();
    } else {
        stop();
        updateCurrentValue(adjustedToReal(rect));
    }
    
    m_adjustedTargetRectValid = true;
}

const QMargins &ViewportAnimator::viewportMargins() const {
    return m_margins;
}

void ViewportAnimator::setViewportMargins(const QMargins &margins) {
    if(margins == m_margins)
        return;

    m_margins = margins;

    if(m_adjustedTargetRectValid)
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
    qreal aspectRatio = SceneUtility::aspectRatio(view()->viewport()->rect());
    QRectF startRect = SceneUtility::expanded(aspectRatio, from.toRectF(), Qt::KeepAspectRatioByExpanding, Qt::AlignCenter);
    QRectF targetRect = SceneUtility::expanded(aspectRatio, to.toRectF(), Qt::KeepAspectRatioByExpanding, Qt::AlignCenter);

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

    MarginsF relativeMargins = SceneUtility::cwiseDiv(m_margins, view->viewport()->size());

    /* Calculate size to match real aspect ratio. */
    QSizeF fixedRealSize = SceneUtility::expanded(SceneUtility::aspectRatio(view->viewport()->size()), realRect.size(), Qt::KeepAspectRatioByExpanding);

    /* Calculate resulting size. */
    QSizeF adjustedSize = fixedRealSize;
    if(m_marginFlags & Qn::MarginsAffectSize)
        adjustedSize = SceneUtility::cwiseMul(adjustedSize, QSizeF(1.0, 1.0) - SceneUtility::sizeDelta(relativeMargins));

    /* Calculate resulting position. */
    QPointF adjustedCenter = realRect.center();
    if(m_marginFlags & Qn::MarginsAffectPosition) {
        QRectF fixedRealRect = QRectF(adjustedCenter - SceneUtility::toPoint(fixedRealSize) / 2, fixedRealSize);

        adjustedCenter = SceneUtility::eroded(fixedRealRect, SceneUtility::cwiseMul(fixedRealRect.size(), relativeMargins)).center();

        /* Adjust so that adjusted rect lies inside the real one. */
        adjustedCenter.rx() = qBound(fixedRealRect.left() + adjustedSize.width()  / 2, adjustedCenter.x(), fixedRealRect.right()  - adjustedSize.width()  / 2);
        adjustedCenter.ry() = qBound(fixedRealRect.top()  + adjustedSize.height() / 2, adjustedCenter.y(), fixedRealRect.bottom() - adjustedSize.height() / 2);
    }

    return QRectF(adjustedCenter - SceneUtility::toPoint(adjustedSize) / 2, adjustedSize);
}

QRectF ViewportAnimator::adjustedToReal(const QGraphicsView *view, const QRectF &adjustedRect) const {
    if(view == NULL)
        return adjustedRect; /* Graphics view was destroyed, cannot adjust. */

    MarginsF relativeMargins = SceneUtility::cwiseDiv(m_margins, view->viewport()->size());

    /* Calculate margins that can be used for expanding adjusted to real rect. 
     * Note that these margins are still positive, so SceneUtility::dilated should be used with them. */
    MarginsF inverseRelativeMargins = SceneUtility::cwiseDiv(relativeMargins, QSize(1.0, 1.0) - SceneUtility::sizeDelta(relativeMargins));

    /* Calculate size to match adjusted aspect ratio. */
    QSizeF fixedAdjustedSize;
    {
        QSize adjustedViewportSize = view->viewport()->size();
        if(m_marginFlags & Qn::MarginsAffectSize)
            adjustedViewportSize = SceneUtility::eroded(adjustedViewportSize, m_margins);

        fixedAdjustedSize = SceneUtility::expanded(SceneUtility::aspectRatio(adjustedViewportSize), adjustedRect.size(), Qt::KeepAspectRatioByExpanding);
    }

    /* Calculate resulting size. */
    QSizeF realSize = fixedAdjustedSize;
    if(m_marginFlags & Qn::MarginsAffectSize)
        realSize = SceneUtility::cwiseMul(realSize, QSizeF(1.0, 1.0) + SceneUtility::sizeDelta(inverseRelativeMargins));

    /* Calculate resulting position. */
    QPointF realCenter = adjustedRect.center();
    if(m_marginFlags & Qn::MarginsAffectPosition) {
        QRectF fixedAdjustedRect = QRectF(realCenter - SceneUtility::toPoint(fixedAdjustedSize) / 2, fixedAdjustedSize);

        realCenter = SceneUtility::dilated(fixedAdjustedRect, SceneUtility::cwiseMul(fixedAdjustedRect.size(), inverseRelativeMargins)).center();

        /* Adjust so that adjusted rect lies inside the real one. */
        realCenter.rx() = qBound(adjustedRect.right()  - realSize.width()  / 2, realCenter.x(), adjustedRect.left() + realSize.width()  / 2);
        realCenter.ry() = qBound(adjustedRect.bottom() - realSize.height() / 2, realCenter.y(), adjustedRect.top()  + realSize.height() / 2);
    }

    return QRectF(realCenter - SceneUtility::toPoint(realSize) / 2, realSize);
}
 