#include "fisheye_ptz_controller.h"
#include "ui/graphics/items/resource/resource_widget_renderer.h"
#include <math.h>
#include "utils/math/space_mapper.h"
#include "core/resource/interface/abstract_ptz_controller.h"
#include "core/resource/media_resource.h"

qreal MAX_MOVE_SPEED = 1.0; // 1 rad per second
qreal MAX_ZOOM_SPEED = gradToRad(30.0); // zoom speed
qreal MIN_FOV = gradToRad(20.0);
qreal MAX_FOV = gradToRad(180.0);

qreal FISHEYE_FOV = gradToRad(180.0);

qreal MOVETO_ANIMATION_TIME = 0.5;

QnFisheyePtzController::QnFisheyePtzController(QnResource* resource):
    QnVirtualPtzController(resource),
    m_renderer(0),
    m_lastTime(0),
    m_moveToAnimation(false),
    m_spaceMapper(0)
{
    updateSpaceMapper(DewarpingParams().horizontalView);
}

QnFisheyePtzController::~QnFisheyePtzController()
{
    if (m_renderer)
        m_renderer->setFisheyeController(0);
    delete m_spaceMapper;
}

void QnFisheyePtzController::updateSpaceMapper(bool horizontalView)
{
    if (m_spaceMapper)
        delete m_spaceMapper;

    QnScalarSpaceMapper zMapper(fovTo35mmEquiv(MIN_FOV), fovTo35mmEquiv(MAX_FOV), fovTo35mmEquiv(MIN_FOV), fovTo35mmEquiv(MAX_FOV), Qn::ConstantExtrapolation);
    qreal minX, maxX, minY, maxY;
    Qn::ExtrapolationMode xExtrapolationMode;

    if (horizontalView) {
        minX = minY = -180.0;
        maxX = maxY = 180.0;
        xExtrapolationMode = Qn::ConstantExtrapolation;
    }
    else {
        minX = -180.0;
        maxX = 180.0;
        minY = 0.0;
        maxY = 180.0;
        xExtrapolationMode = Qn::PeriodicExtrapolation;
    }

    QnScalarSpaceMapper xMapper(minX, maxX, minX, maxX, xExtrapolationMode);
    QnScalarSpaceMapper yMapper(minY, maxY, minY, maxY, Qn::ConstantExtrapolation);
    m_spaceMapper = new QnPtzSpaceMapper(QnVectorSpaceMapper(xMapper, yMapper, zMapper), QStringList());

    emit spaceMapperChanged();
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

qreal QnFisheyePtzController::boundXAngle(qreal value, qreal fov) const
{
    //qreal yRange = xRange / m_dstPos.aspectRatio;
    qreal xRange = (FISHEYE_FOV - fov) / 2.0;
    if (m_dewarpingParams.horizontalView)
        return qBound(-xRange, value, xRange);
    else
        return value;
}

qreal QnFisheyePtzController::boundYAngle(qreal value, qreal fov, qreal aspectRatio) const
{
    qreal yRange = (FISHEYE_FOV - fov) / 2.0 / aspectRatio;
    if (m_dewarpingParams.horizontalView)
        return qBound(-yRange, value, yRange);
    else
        return qBound(0.0, value, yRange / 2.0);
}

int QnFisheyePtzController::moveTo(qreal xPos, qreal yPos, qreal zoomPos)
{
    m_motion = QVector3D();

    m_dstPos.fov = qBound(MIN_FOV, mm35vToFov(zoomPos), MAX_FOV);

    m_dstPos.xAngle = boundXAngle(gradToRad(xPos), m_dstPos.fov);
    m_dstPos.yAngle = boundYAngle(gradToRad(yPos), m_dstPos.fov, 1.0);
    m_srcPos = m_dewarpingParams;

    if (m_dstPos.xAngle - m_srcPos.xAngle > M_PI)
        m_srcPos.xAngle += M_PI * 2.0;
    else if (m_dstPos.xAngle - m_srcPos.xAngle < -M_PI)
        m_srcPos.xAngle -= M_PI * 2.0;

    m_moveToAnimation = true;
    return 0;
}

int QnFisheyePtzController::getPosition(qreal *xPos, qreal *yPos, qreal *zoomPos)
{
    *xPos = radToGrad(m_dewarpingParams.xAngle);
    *yPos = radToGrad(m_dewarpingParams.yAngle);
    *zoomPos = fovTo35mmEquiv(m_dewarpingParams.fov);

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

qreal normalizeAngle(qreal value)
{
    if (value > M_PI * 2)
        return value - M_PI * 2;
    else if(value < -M_PI * 2)
        return value + M_PI * 2;
    else
        return value;
}

DewarpingParams QnFisheyePtzController::getDewarpingParams()
{
    qint64 newTime = getUsecTimer();
    qreal timeSpend = (newTime - m_lastTime) / 1000000.0;
    DewarpingParams newParams = m_dewarpingParams;

    if (m_moveToAnimation)
    {
        if (timeSpend < MOVETO_ANIMATION_TIME && m_animate) 
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

        newParams.xAngle = normalizeAngle(newParams.xAngle + xSpeed * timeSpend);
        newParams.yAngle += ySpeed * timeSpend;

        qreal xRange = (FISHEYE_FOV - newParams.fov) / 2.0;
        qreal yRange = xRange; // / newParams.aspectRatio;

        newParams.xAngle = boundXAngle(newParams.xAngle, newParams.fov);
        newParams.yAngle = boundYAngle(newParams.yAngle, newParams.fov, 1.0);
        m_lastTime = newTime;
    }
    newParams.enabled = m_dewarpingParams.enabled;

    DewarpingParams camParams = m_resource->getDewarpingParams();
    newParams.fovRot = gradToRad(camParams.fovRot);
    newParams.horizontalView = camParams.horizontalView;

    if (!(newParams == m_dewarpingParams)) 
    {
        if (newParams.horizontalView != m_dewarpingParams.horizontalView)
            updateSpaceMapper(newParams.horizontalView);
        emit dewarpingParamsChanged(newParams);
        m_dewarpingParams = newParams;
    }

    //newParams.fovRot = gradToRad(-12.0); // city 360 picture
    //newParams.fovRot = gradToRad(-18.0);
    return newParams;
}

void QnFisheyePtzController::setEnabled(bool value)
{
    if (m_dewarpingParams.enabled != value) {
        m_dewarpingParams.enabled = value;
        emit dewarpingParamsChanged(m_dewarpingParams);
    }
}

bool QnFisheyePtzController::isEnabled() const
{
    return m_dewarpingParams.enabled;
}

void QnFisheyePtzController::moveToRect(const QRectF& r)
{
    QPointF c = QPointF(r.center().x() - 0.5, r.center().y() - 0.5);
    qreal fov = r.width() * M_PI;
    if (m_resource->getDewarpingParams().horizontalView) {
        qreal x = c.x() * M_PI;
        qreal y = -c.y() * M_PI;
        moveTo(radToGrad(x), radToGrad(y), fovTo35mmEquiv(fov));
    }
    else {
        qreal x = -(::atan2(c.y(), c.x()) - M_PI/2.0);
        qreal y = 0;
        if (qAbs(c.x()) > qAbs(c.y())) 
        {
            if (c.x() > 0)
                y = (1.0 - r.right()) *  M_PI;
            else
                y = r.left() * M_PI;
        }
        else {
            if (c.y() > 0)
                y = (1.0 - r.bottom()) * M_PI;
            else
                y = r.top() * M_PI;
        }

        moveTo(radToGrad(x), radToGrad(y), fovTo35mmEquiv(fov));
    }
}
