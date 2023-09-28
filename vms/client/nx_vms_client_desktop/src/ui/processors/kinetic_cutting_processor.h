// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_KINETIC_ZOOM_PROCESSOR_H
#define QN_KINETIC_ZOOM_PROCESSOR_H

#include <cmath>
#include <limits>

#include "kinetic_processor.h"

/**
 * Kinetic processor that introduces speed cutting threshold
 * after which speed is cut much more aggressively.
 */
class KineticCuttingProcessor: public KineticProcessor {
    typedef KineticProcessor base_type;

public:
    KineticCuttingProcessor(QMetaType type, QObject *parent = nullptr):
        base_type(type, parent),
        m_speedCuttingThreshold(std::numeric_limits<qreal>::max())
    {}


    /**
     * \param speedCuttingThreshold     Threshold after which speed will be cut
     *                                  much more aggressively.
     */
    void setSpeedCuttingThreshold(qreal speedCuttingThreshold) {
        m_speedCuttingThreshold = speedCuttingThreshold;
    }

    qreal speedCuttingThreshold() const {
        return m_speedCuttingThreshold;
    }

protected:
    virtual QVariant updateSpeed(const QVariant &initialSpeed, const QVariant &currentSpeed, const QVariant &speedGain, qreal dt) const override {
        QVariant speed = base_type::updateSpeed(initialSpeed, currentSpeed, speedGain, dt);

        qreal oldMagnitude = magnitudeCalculator()->calculate(speed);
        if(oldMagnitude > m_speedCuttingThreshold) {
            qreal newMagnitude = m_speedCuttingThreshold * std::pow(oldMagnitude / m_speedCuttingThreshold, 1 / (1 + friction() / m_speedCuttingThreshold * dt));
            speed = linearCombinator()->combine(newMagnitude / oldMagnitude, speed);
        }

        return speed;
    }

private:
    qreal m_speedCuttingThreshold;
};

#endif // QN_KINETIC_ZOOM_PROCESSOR_H
