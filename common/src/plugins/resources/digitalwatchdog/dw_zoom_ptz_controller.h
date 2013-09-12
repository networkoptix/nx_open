#ifndef QN_DW_ZOOM_PTZ_CONTROLLER_H
#define QN_DW_ZOOM_PTZ_CONTROLLER_H

#include <core/ptz/abstract_ptz_controller.h>

class QnDwZoomPtzController: public QnAbstractPtzController {
    Q_OBJECT;
public:
    QnDwZoomPtzController(const QnPlWatchDogResourcePtr &resource);
    virtual ~QnDwZoomPtzController();

    virtual int startMove(const QVector3D &speed) override;
    virtual int stopMove() override;
    virtual int setPosition(const QVector3D &position) override;
    virtual int getPosition(QVector3D *position) override;
    virtual Qn::PtzCapabilities getCapabilities() override;

private:
    QnPlWatchDogResourcePtr m_resource;
};

#endif // QN_DW_ZOOM_PTZ_CONTROLLER_H
