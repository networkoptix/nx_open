#ifndef QN_DW_ZOOM_PTZ_CONTROLLER_H
#define QN_DW_ZOOM_PTZ_CONTROLLER_H

#ifdef ENABLE_ONVIF
#include <core/ptz/abstract_ptz_controller.h>


class QnDwZoomPtzController: public QnAbstractPtzController {
    Q_OBJECT;
public:
    QnDwZoomPtzController(const QnPlWatchDogResourcePtr &resource);
    virtual ~QnDwZoomPtzController();

    virtual Qn::PtzCapabilities getCapabilities() override;
    virtual bool continuousMove(const QVector3D &speed) override;
    virtual bool absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position) override;
    virtual bool getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) override;

    virtual bool getLimits(QnPtzLimits *limits) override;
    virtual bool getFlip(Qt::Orientations *flip) override;
    virtual bool relativeMove(qreal aspectRatio, const QRectF &viewport) override;

private:
    QnPlWatchDogResourcePtr m_resource;
};

#endif // ENABLE_ONVIF

#endif // QN_DW_ZOOM_PTZ_CONTROLLER_H
