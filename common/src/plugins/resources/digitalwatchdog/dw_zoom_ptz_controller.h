#ifndef QN_DW_ZOOM_PTZ_CONTROLLER_H
#define QN_DW_ZOOM_PTZ_CONTROLLER_H

#include <core/resource/interface/abstract_ptz_controller.h>


class QnDwZoomPtzController: public QnAbstractPtzController {
    Q_OBJECT;
public:
    QnDwZoomPtzController(const QnPlWatchDogResourcePtr &resource, QObject *parent = NULL);
    virtual ~QnDwZoomPtzController();

    virtual int startMove(qreal xVelocity, qreal yVelocity, qreal zoomVelocity) override;
    virtual int stopMove() override;
    virtual int moveTo(qreal xPos, qreal yPos, qreal zoomPos) override;
    virtual int getPosition(qreal *xPos, qreal *yPos, qreal *zoomPos) override;
    virtual Qn::CameraCapabilities getCapabilities() override;
    virtual const QnPtzSpaceMapper *getSpaceMapper() override;

private:
    QnPlWatchDogResourcePtr m_resource;
};



#endif // QN_DW_ZOOM_PTZ_CONTROLLER_H
