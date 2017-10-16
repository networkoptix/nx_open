#include "rect_animator.h"
#include <cmath> /* For std::abs, std::exp & std::log. */
#include <QtCore/QRectF>
#include <utils/common/warnings.h>
#include <nx/client/core/utils/geometry.h>

using nx::client::core::utils::Geometry;

RectAnimator::RectAnimator(QObject *parent):
    base_type(parent),
    m_logScalingSpeed(std::log(2.0)),
    m_relativeMovementSpeed(1.0),
    m_absoluteMovementSpeed(0.0)
{}

RectAnimator::~RectAnimator() {}

void RectAnimator::updateInternalType(int newType) {
    if(newType != QMetaType::UnknownType && newType != QMetaType::QRectF)
        qnWarning("Type '%1' is not supported by this animator.", QMetaType::typeName(newType));

    base_type::updateInternalType(newType);
}

int RectAnimator::estimatedDuration(const QVariant &from, const QVariant &to) const {
    if(internalType() != QMetaType::QRectF)
        return base_type::estimatedDuration(from, to);

    QRectF startRect = from.toRectF();
    QRectF targetRect = to.toRectF();

    static const qreal eps = 1.0e-6;

    qreal startDiagonal = Geometry::length(startRect.size());
    qreal targetDiagonal = Geometry::length(targetRect.size());

    /* Formulas below don't do well with zero diagonals, so we adjust them. */
    if(qFuzzyIsNull(startDiagonal)) {
        if(qFuzzyIsNull(targetDiagonal)) {
            startDiagonal = targetDiagonal = eps;
        } else {
            startDiagonal = targetDiagonal * eps;
        }
    } else if(qFuzzyIsNull(targetDiagonal)) {
        targetDiagonal = startDiagonal * eps;
    }

    qreal movementSpeed =
        absoluteMovementSpeed() + relativeMovementSpeed() * (startDiagonal + targetDiagonal) / 2;
    qreal movementTime =
        Geometry::length(targetRect.center() - startRect.center()) / movementSpeed;

    qreal scalingTime = std::abs(std::log(startDiagonal / targetDiagonal)) / m_logScalingSpeed;

    return qMax(scalingTime, movementTime) * 1000;
}

void RectAnimator::setScalingSpeed(qreal scalingSpeed) {
    if(scalingSpeed <= 1.0) {
        qnWarning("Invalid non-positive scaling speed %1.", scalingSpeed);
        return;
    }

    if(isRunning()) {
        qnWarning("Cannot change scaling speed of a running animator.");
        return;
    }

    m_logScalingSpeed = std::log(scalingSpeed);
    invalidateDuration();
}

qreal RectAnimator::scalingSpeed() const {
    return std::exp(m_logScalingSpeed);
}

void RectAnimator::setRelativeMovementSpeed(qreal relativeMovementSpeed) {
    if(relativeMovementSpeed < 0.0) {
        qnWarning("Invalid negative movement speed %1.", relativeMovementSpeed);
        return;
    }

    if(isRunning()) {
        qnWarning("Cannot change movement speed of a running animator.");
        return;
    }

    m_relativeMovementSpeed = relativeMovementSpeed;
    invalidateDuration();
}

void RectAnimator::setAbsoluteMovementSpeed(qreal absoluteMovementSpeed) {
    if(absoluteMovementSpeed < 0.0) {
        qnWarning("Invalid negative absolute movement speed %1.", absoluteMovementSpeed);
        return;
    }

    if(isRunning()) {
        qnWarning("Cannot change movement speed of a running animator.");
        return;
    }

    m_absoluteMovementSpeed = absoluteMovementSpeed;
    invalidateDuration();
}
