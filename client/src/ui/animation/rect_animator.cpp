#include "rect_animator.h"
#include <cmath> /* For std::abs, std::exp & std::log. */
#include <QRectF>
#include <utils/common/warnings.h>
#include <ui/common/scene_utility.h>

QnRectAnimator::QnRectAnimator(QObject *parent):
    base_type(parent),
    m_logScalingSpeed(std::log(2.0)),
    m_relativeMovementSpeed(1.0),
    m_absoluteMovementSpeed(0.0)
{}

QnRectAnimator::~QnRectAnimator() {}

void QnRectAnimator::updateInternalType(int newType) {
    if(newType != QMetaType::Void && newType != QMetaType::QRectF)
        qnWarning("Type '%1' is not supported by this animator.", QMetaType::typeName(newType));

    base_type::updateInternalType(newType);
}

int QnRectAnimator::estimatedDuration(const QVariant &from, const QVariant &to) const {
    if(internalType() != QMetaType::QRectF)
        return base_type::estimatedDuration(from, to);

    QRectF startRect = from.toRectF();
    QRectF targetRect = to.toRectF();

    qreal startDiagonal = SceneUtility::length(startRect.size());
    qreal targetDiagonal = SceneUtility::length(targetRect.size());

    qreal movementSpeed = absoluteMovementSpeed() + relativeMovementSpeed() * (startDiagonal + targetDiagonal) / 2;
    qreal movementTime = SceneUtility::length(targetRect.center() - startRect.center()) / movementSpeed;

    qreal scalingTime = std::abs(std::log(startDiagonal / targetDiagonal)) / m_logScalingSpeed;

    return qMax(scalingTime, movementTime) * 1000;
}

void QnRectAnimator::setScalingSpeed(qreal scalingSpeed) {
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

qreal QnRectAnimator::scalingSpeed() const {
    return std::exp(m_logScalingSpeed);
}

void QnRectAnimator::setRelativeMovementSpeed(qreal relativeMovementSpeed) {
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

void QnRectAnimator::setAbsoluteMovementSpeed(qreal absoluteMovementSpeed) {
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
