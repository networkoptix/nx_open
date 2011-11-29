#ifndef QN_KINETIC_ZOOM_PROCESSOR_H
#define QN_KINETIC_ZOOM_PROCESSOR_H

#include "kineticprocessor.h"
#include <limits>

/**
 * Kinetic processor that introduces speed cutting threshold 
 * after which speed is cut much more aggressively.
 */
template<class T>
class KineticCuttingProcessor: public KineticProcessor<T> {
    typedef KineticProcessor<T> base_type;

public:
    KineticCuttingProcessor(QObject *parent = NULL):
        base_type(parent),
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
    virtual T updateSpeed(const T &initialSpeed, const T &currentSpeed, const T &speedGain, qreal dt) const override {
        T speed = base_type::updateSpeed(initialSpeed, currentSpeed, speedGain, dt);

        qreal oldMagnitude = magnitude(speed);
        if(oldMagnitude > m_speedCuttingThreshold) {
            qreal newMagnitude = m_speedCuttingThreshold * std::pow(oldMagnitude / m_speedCuttingThreshold, 1 / (1 + friction() / m_speedCuttingThreshold * dt));
            speed = speed * (newMagnitude / oldMagnitude);
        }

        return speed;
    }

private:
    qreal m_speedCuttingThreshold;
};

#endif // QN_KINETIC_ZOOM_PROCESSOR_H
