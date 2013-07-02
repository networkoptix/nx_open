#ifndef __FISHEYE_PTZ_CONTROLLER_H__
#define __FISHEYE_PTZ_CONTROLLER_H__

#include <QVector3D>
#include "ui/fisheye/virtual_ptz_controller.h"
#include <math.h>

class QnResourceWidgetRenderer;

struct DevorpingParams
{
    DevorpingParams(): enabled(false), xAngle(0.0), yAngle(0.0), fov(M_PI/2.0), pAngle(0.0), aspectRatio(1.0) {}

    bool enabled;
    // view angle and FOV at radians
    float xAngle;
    float yAngle;
    float fov;
    // perspective correction angle
    float pAngle;
    float aspectRatio;
};

class QnFisheyePtzController: public QnVirtualPtzController
{
public:
    QnFisheyePtzController();
    virtual ~QnFisheyePtzController();
    void addRenderer(QnResourceWidgetRenderer* renderer);

    virtual void setMovement(const QVector3D& motion) override;
    virtual QVector3D movement() override;

    void setAspectRatio(float aspectRatio);
    DevorpingParams getDevorpingParams();
private slots:
    void at_timer();
private:
    QVector3D m_motion;
    QnResourceWidgetRenderer* m_renderer;
    DevorpingParams m_devorpingParams;
    qint64 m_lastTime;
};

#endif // __FISHEYE_PTZ_CONTROLLER_H__
