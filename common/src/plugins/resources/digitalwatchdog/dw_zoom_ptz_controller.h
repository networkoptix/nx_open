#ifndef QN_DW_ZOOM_PTZ_CONTROLLER_H
#define QN_DW_ZOOM_PTZ_CONTROLLER_H

#include <core/ptz/abstract_ptz_controller.h>
#include <core/resource/media_resource.h>


class QnDwZoomPtzController: public QnAbstractPtzController {
    Q_OBJECT;
public:
    QnDwZoomPtzController(QnPlWatchDogResource* resource);
    virtual ~QnDwZoomPtzController();

    virtual int startMove(qreal xVelocity, qreal yVelocity, qreal zoomVelocity) override;
    virtual int stopMove() override;
    virtual int moveTo(qreal xPos, qreal yPos, qreal zoomPos) override;
    virtual int getPosition(qreal *xPos, qreal *yPos, qreal *zoomPos) override;
    virtual Qn::PtzCapabilities getCapabilities() override;
    virtual const QnPtzSpaceMapper *getSpaceMapper() override;

private:
    QnPlWatchDogResource* m_resource;
};



#endif // QN_DW_ZOOM_PTZ_CONTROLLER_H
