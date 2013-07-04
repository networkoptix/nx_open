#ifndef __FISHEYE_PTZ_CONTROLLER_H__
#define __FISHEYE_PTZ_CONTROLLER_H__

#include <QVector3D>
#include <math.h>
#include "core/resource/interface/abstract_ptz_controller.h"

class QnResourceWidgetRenderer;

struct DevorpingParams
{
    DevorpingParams(): enabled(false), xAngle(0.0), yAngle(0.0), fov(M_PI/2.0), pAngle(0.0), aspectRatio(1.0) {}

    bool enabled;
    // view angle and FOV at radians
    qreal xAngle;
    qreal yAngle;
    qreal fov;
    // perspective correction angle
    qreal pAngle;
    qreal aspectRatio;
};

class QnFisheyePtzController: public QnAbstractPtzController
{
public:
    QnFisheyePtzController(QnResource* resource);
    virtual ~QnFisheyePtzController();

    virtual int startMove(qreal xVelocity, qreal yVelocity, qreal zoomVelocity) override;
    virtual int stopMove() override;
    virtual int moveTo(qreal xPos, qreal yPos, qreal zoomPos) override;
    virtual int getPosition(qreal *xPos, qreal *yPos, qreal *zoomPos) override;
    virtual Qn::PtzCapabilities getCapabilities() override;
    virtual const QnPtzSpaceMapper *getSpaceMapper() override;

    void addRenderer(QnResourceWidgetRenderer* renderer);
    void setAspectRatio(float aspectRatio);
    DevorpingParams getDevorpingParams();
private slots:
    void at_timer();
private:
    QVector3D m_motion;
    QnResourceWidgetRenderer* m_renderer;
    DevorpingParams m_devorpingParams;
    DevorpingParams m_srcPos;
    DevorpingParams m_dstPos;
    bool m_moveToAnimation;
    qint64 m_lastTime;
    QnPtzSpaceMapper* m_spaceMapper;
};

#endif // __FISHEYE_PTZ_CONTROLLER_H__
