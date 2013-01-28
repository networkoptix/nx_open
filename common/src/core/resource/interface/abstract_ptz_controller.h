#ifndef QN_ABSTRACT_PTZ_CONTROLLER_H
#define QN_ABSTRACT_PTZ_CONTROLLER_H

#include <QObject>

#include <common/common_globals.h>

#include <core/resource/resource_fwd.h>

class QnAbstractPtzController: public QObject {
    Q_OBJECT
public:
    QnAbstractPtzController(const QnResourcePtr &resource, QObject *parent = NULL);

    virtual int startMove(qreal xVelocity, qreal yVelocity, qreal zoomVelocity) = 0;
    virtual int moveTo(qreal xPos, qreal yPos, qreal zoomPos) = 0;
    virtual int getPosition(qreal *xPos, qreal *yPos, qreal *zoomPos) = 0;
    virtual int stopMove() = 0;
    virtual Qn::CameraCapabilities getCapabilities() = 0;

    bool calibrate(qreal xVelocityCoeff, qreal yVelocityCoeff, qreal zoomVelocityCoeff);
    void getCalibrate(qreal &xVelocityCoeff, qreal &yVelocityCoeff, qreal &zoomVelocityCoeff);

    static bool calibrate(QnVirtualCameraResourcePtr res, qreal xVelocityCoeff, qreal yVelocityCoeff, qreal zoomVelocityCoeff);
    static void getCalibrate(QnVirtualCameraResourcePtr res, qreal &xVelocityCoeff, qreal &yVelocityCoeff, qreal &zoomVelocityCoeff);

    qreal getXVelocityCoeff() const;
    qreal getYVelocityCoeff() const;
    qreal getZoomVelocityCoeff() const;

private:
    QnVirtualCameraResourcePtr m_resource; // TODO: #VASILENKO why not QnSecurityCamResource?
};

#endif // QN_ABSTRACT_PTZ_CONTROLLER_H
