#include "kineticzoomprocessor.h"
#include <limits>

KineticZoomProcessor::KineticZoomProcessor(QObject *parent):
    base_type(parent),
    mSpeedCuttingThreshold(std::numeric_limits<qreal>::max())
{}

qreal KineticZoomProcessor::updateSpeed(const qreal &initialSpeed, const qreal &currentSpeed, const qreal &speedGain, qreal dt) const {
    qreal speed = base_type::updateSpeed(initialSpeed, currentSpeed, speedGain, dt);

    qreal oldMagnitude = magnitude(speed);
    if(oldMagnitude > mSpeedCuttingThreshold) {
        qreal newMagnitude = mSpeedCuttingThreshold * std::pow(oldMagnitude / mSpeedCuttingThreshold, 1 / (1 + friction() / mSpeedCuttingThreshold * dt));
        speed = speed * (newMagnitude / oldMagnitude);
    }

    return speed;
}

