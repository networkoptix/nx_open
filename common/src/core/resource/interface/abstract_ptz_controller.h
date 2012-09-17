#ifndef __QNABSTRACT_PTZ_CONTROLLER_H__
#define __QNABSTRACT_PTZ_CONTROLLER_H__

#include <QObject>
#include <QSize>
#include "core/resource/resource_fwd.h"

class QnAbstractPtzController: public QObject
{
public:
    QnAbstractPtzController(QnResourcePtr netRes);

    virtual int startMove(qreal xVelocity, qreal yVelocity, qreal zoomVelocity) = 0;
    virtual int stopMove() = 0;

    bool calibrate(qreal xVelocityCoeff, qreal yVelocityCoeff, qreal zoomVelocityCoeff);
    void getCalibrate(qreal &xVelocityCoeff, qreal &yVelocityCoeff, qreal &zoomVelocityCoeff);

    static bool calibrate(QnVirtualCameraResourcePtr res, qreal xVelocityCoeff, qreal yVelocityCoeff, qreal zoomVelocityCoeff);
    static void getCalibrate(QnVirtualCameraResourcePtr res, qreal &xVelocityCoeff, qreal &yVelocityCoeff, qreal &zoomVelocityCoeff);

    qreal getXVelocityCoeff() const;
    qreal getYVelocityCoeff() const;
    qreal getZoomVelocityCoeff() const;
private:
    QnVirtualCameraResourcePtr m_resource;
};

#endif //__QNABSTRACT_PTZ_CONTROLLER_H__
