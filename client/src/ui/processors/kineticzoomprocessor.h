#ifndef QN_KINETIC_ZOOM_PROCESSOR_H
#define QN_KINETIC_ZOOM_PROCESSOR_H

#include "kineticprocessor.h"

/**
 * Kinetic processor for zooming.
 * 
 * Introduces speed cutting threshold, after which speed is cut much more aggressively.
 */
class KineticZoomProcessor: public KineticProcessor<qreal> {
    typedef KineticProcessor<qreal> base_type;

public:
    KineticZoomProcessor(QObject *parent = NULL);

    /**
     * \param speedCuttingThreshold     Threshold after which speed will be cut
     *                                  much more aggressively.
     */
    void setSpeedCuttingThreshold(qreal speedCuttingThreshold) {
        mSpeedCuttingThreshold = speedCuttingThreshold;
    }

    qreal speedCuttingThreshold() const {
        return mSpeedCuttingThreshold;
    }

protected:
    virtual qreal updateSpeed(const qreal &initialSpeed, const qreal &currentSpeed, const qreal &speedGain, qreal dt) const override;

private:
    qreal mSpeedCuttingThreshold;
};

#endif // QN_KINETIC_ZOOM_PROCESSOR_H
