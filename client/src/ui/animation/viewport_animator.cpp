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
    m_relativeSpeed(0.0)
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

void ViewportAnimator::moveTo(const QRectF &rect) {
    pause();
    
    setTargetValue(rect);

    start();
}

const QMargins &ViewportAnimator::viewportMargins() const {
    return m_accessor->margins();
}

void ViewportAnimator::setViewportMargins(const QMargins &margins) {
    if(margins == m_accessor->margins())
        return;

    QRectF targetValue = this->targetValue().toRectF();

    m_accessor->setMargins(margins);

    if(isRunning()) {
        /* Margins were changed, restart needed to avoid jitter. */
        pause();
        setTargetValue(targetValue);
        start();
    }
}

Qn::MarginFlags ViewportAnimator::marginFlags() const {
    return m_accessor->marginFlags();
}

void ViewportAnimator::setMarginFlags(Qn::MarginFlags marginFlags) {
    if(marginFlags == m_accessor->marginFlags())
        return;

    QRectF targetValue = this->targetValue().toRectF();

    m_accessor->setMarginFlags(marginFlags);

    if(isRunning()) {
        /* Margin flags were changed, restart needed to avoid jitter. */
        pause();
        setTargetValue(targetValue);
        start();
    }
}

int ViewportAnimator::estimatedDuration(const QVariant &from, const QVariant &to) const {
    QGraphicsView *view = this->view();
    QRectF startRect = m_accessor->adjustedToViewport(view, from.toRectF());
    QRectF targetRect = m_accessor->adjustedToViewport(view, to.toRectF());

    return base_type::estimatedDuration(startRect, targetRect);
}

QRectF ViewportAnimator::targetRect() const {
    return targetValue().toRectF();
}

