#include "fisheye_ptz_processor.h"
#include "ui/graphics/items/resource/resource_widget_renderer.h"
#include <math.h>
#include "utils/math/space_mapper.h"

qreal MAX_MOVE_SPEED = 1.0; // 1 rad per second
qreal MAX_ZOOM_SPEED = gradToRad(30.0); // zoom speed
qreal MIN_FOV = gradToRad(30.0);
qreal MAX_FOV = gradToRad(90.0);

qreal FISHEYE_FOV = gradToRad(180.0);

qreal MOVETO_ANIMATION_TIME = 0.5;

QnFisheyePtzController::QnFisheyePtzController(QnResource* resource):
    QnAbstractPtzController(resource),
    m_renderer(0),
    m_lastTime(0),
    m_moveToAnimation(false)
{
    QnScalarSpaceMapper xMapper(-90.0, 90.0, -90.0, 90.0, Qn::ConstantExtrapolation);
    QnScalarSpaceMapper yMapper(-90.0, 90.0, -90.0, 90.0, Qn::ConstantExtrapolation);
    QnScalarSpaceMapper zMapper(fovTo35mmEquiv(MIN_FOV), fovTo35mmEquiv(MAX_FOV), fovTo35mmEquiv(MIN_FOV), fovTo35mmEquiv(MAX_FOV), Qn::ConstantExtrapolation);

    m_spaceMapper = new QnPtzSpaceMapper(QnVectorSpaceMapper(xMapper, yMapper, zMapper), QStringList());
}

QnFisheyePtzController::~QnFisheyePtzController()
{
    if (m_renderer)
        m_renderer->setFisheyeController(0);
    delete m_spaceMapper;
}

void QnFisheyePtzController::addRenderer(QnResourceWidgetRenderer* renderer)
{
    m_renderer = renderer;
    m_renderer->setFisheyeController(this);
}

int QnFisheyePtzController::startMove(qreal xVelocity, qreal yVelocity, qreal zoomVelocity)
{
    m_motion = QVector3D(xVelocity, yVelocity, zoomVelocity);
    m_lastTime = getUsecTimer();
    m_moveToAnimation = false;
    return 0;
}

int QnFisheyePtzController::stopMove()
{
    m_motion = QVector3D();
    return 0;
}

int QnFisheyePtzController::moveTo(qreal xPos, qreal yPos, qreal zoomPos)
{
    m_motion = QVector3D();

    m_dstPos.fov = qBound(MIN_FOV, mm35vToFov(zoomPos), MAX_FOV);

    qreal xRange = (FISHEYE_FOV - m_dstPos.fov) / 2.0;
    qreal yRange = xRange / m_dstPos.aspectRatio;

    m_dstPos.xAngle = qBound(-xRange, gradToRad(xPos), xRange);
    m_dstPos.yAngle = qBound(-yRange, gradToRad(yPos), yRange);
    m_srcPos = m_devorpingParams;
    m_moveToAnimation = true;
    return 0;
}

int QnFisheyePtzController::getPosition(qreal *xPos, qreal *yPos, qreal *zoomPos)
{
    *xPos = radToGrad(m_devorpingParams.xAngle);
    *yPos = radToGrad(m_devorpingParams.yAngle);
    *zoomPos = fovTo35mmEquiv(m_devorpingParams.fov);

    return 0;
}


Qn::PtzCapabilities QnFisheyePtzController::getCapabilities()
{
    return Qn::AllPtzCapabilities;
}

const QnPtzSpaceMapper* QnFisheyePtzController::getSpaceMapper()
{
    return m_spaceMapper;
}


void QnFisheyePtzController::at_timer()
{

}

void QnFisheyePtzController::setAspectRatio(float aspectRatio)
{
    m_devorpingParams.aspectRatio = aspectRatio;
}

DevorpingParams QnFisheyePtzController::getDevorpingParams()
{
    qint64 newTime = getUsecTimer();
    qreal timeSpend = (newTime - m_lastTime) / 1000000.0;
    DevorpingParams newParams = m_devorpingParams;

    if (m_moveToAnimation)
    {
        if (timeSpend < MOVETO_ANIMATION_TIME) 
        {
            QEasingCurve easing(QEasingCurve::InOutQuad);
            qreal value = easing.valueForProgress(timeSpend / MOVETO_ANIMATION_TIME);

            qreal fovDelta = (m_dstPos.fov - m_srcPos.fov) * value;
            qreal xDelta = (m_dstPos.xAngle - m_srcPos.xAngle) * value;
            qreal yDelta = (m_dstPos.yAngle - m_srcPos.yAngle) * value;

            newParams.fov = m_srcPos.fov + fovDelta;
            newParams.xAngle = m_srcPos.xAngle + xDelta;
            newParams.yAngle = m_srcPos.yAngle + yDelta;
        }
        else {
            newParams = m_dstPos;
            m_moveToAnimation = false;
        }
    }
    else {
        qreal zoomSpeed = -m_motion.z() * MAX_ZOOM_SPEED;
        qreal xSpeed = m_motion.x() * MAX_MOVE_SPEED;
        qreal ySpeed = m_motion.y() * MAX_MOVE_SPEED;

        newParams.fov += zoomSpeed * timeSpend;
        newParams.fov = qBound(MIN_FOV, newParams.fov, MAX_FOV);

        newParams.xAngle += xSpeed * timeSpend;
        newParams.yAngle += ySpeed * timeSpend;

        qreal xRange = (FISHEYE_FOV - newParams.fov) / 2.0;
        qreal yRange = xRange; // / newParams.aspectRatio;

        newParams.xAngle = qBound(-xRange, newParams.xAngle, xRange);
        newParams.yAngle = qBound(-yRange, newParams.yAngle, yRange);
        //newParams.pAngle = gradToRad(18.0);
        m_lastTime = newTime;
    }
    if (!(newParams == m_devorpingParams)) {
        m_devorpingParams = newParams;
        emit dewarpingParamsChanged(newParams);
    }

    return newParams;
}

void QnFisheyePtzController::setEnabled(bool value)
{
    if (m_devorpingParams.enabled != value) {
        m_devorpingParams.enabled = value;
        emit dewarpingParamsChanged(m_devorpingParams);
    }
}

bool QnFisheyePtzController::isEnabled() const
{
    return m_devorpingParams.enabled;
}
