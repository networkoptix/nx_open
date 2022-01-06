// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rect_animator.h"

#include <cmath> /* For std::abs, std::exp & std::log. */

#include <QtCore/QRectF>

#include <nx/vms/client/core/utils/geometry.h>
#include <nx/utils/log/log.h>

using nx::vms::client::core::Geometry;

RectAnimator::RectAnimator(QObject *parent):
    base_type(parent),
    m_logScalingSpeed(std::log(2.0)),
    m_relativeMovementSpeed(1.0),
    m_absoluteMovementSpeed(0.0)
{}

RectAnimator::~RectAnimator() {}

void RectAnimator::updateInternalType(QMetaType newType)
{
    if (newType.id() != QMetaType::UnknownType && newType.id() != QMetaType::QRectF)
        NX_ASSERT(false, "Type '%1' is not supported by this animator.", newType.name());

    base_type::updateInternalType(newType);
}

int RectAnimator::estimatedDuration(const QVariant &from, const QVariant &to) const {
    if (internalType().id() != QMetaType::QRectF)
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
        NX_ASSERT(false, "Invalid non-positive scaling speed %1.", scalingSpeed);
        return;
    }

    if(isRunning()) {
        NX_ASSERT(false, "Cannot change scaling speed of a running animator.");
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
        NX_ASSERT(false, "Invalid negative movement speed %1.", relativeMovementSpeed);
        return;
    }

    if(isRunning()) {
        NX_ASSERT(false, "Cannot change movement speed of a running animator.");
        return;
    }

    m_relativeMovementSpeed = relativeMovementSpeed;
    invalidateDuration();
}

void RectAnimator::setAbsoluteMovementSpeed(qreal absoluteMovementSpeed) {
    if(absoluteMovementSpeed < 0.0) {
        NX_ASSERT(false, "Invalid negative absolute movement speed %1.", absoluteMovementSpeed);
        return;
    }

    if(isRunning()) {
        NX_ASSERT(false, "Cannot change movement speed of a running animator.");
        return;
    }

    m_absoluteMovementSpeed = absoluteMovementSpeed;
    invalidateDuration();
}
