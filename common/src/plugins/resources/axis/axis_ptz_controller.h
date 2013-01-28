#ifndef QN_AXIS_PTZ_CONTROLLER_H
#define QN_AXIS_PTZ_CONTROLLER_H

#include <core/resource/interface/abstract_ptz_controller.h>

class QnAxisPtzController: public QnAbstractPtzController {
    Q_OBJECT;
public:
    QnAxisPtzController(const QnPlAxisResourcePtr &resource, QObject *parent = NULL);
    virtual ~QnAxisPtzController();

    virtual int startMove(qreal xVelocity, qreal yVelocity, qreal zoomVelocity) override;
    virtual int moveTo(qreal xPos, qreal yPos, qreal zoomPos) override;
    virtual int getPosition(qreal *xPos, qreal *yPos, qreal *zoomPos) override;
    virtual int stopMove() override;
    virtual Qn::CameraCapabilities getCapabilities() override;

private:
    QnPlAxisResourcePtr m_resource;
    Qn::CameraCapabilities m_capabilities;
};


#endif // QN_AXIS_PTZ_CONTROLLER_H
