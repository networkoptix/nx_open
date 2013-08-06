#include "fisheye_ptz_controller.h"
#include "ui/graphics/items/resource/resource_widget_renderer.h"
#include <math.h>
#include "utils/math/space_mapper.h"
#include "core/resource/interface/abstract_ptz_controller.h"
#include "core/resource/media_resource.h"

qreal MAX_MOVE_SPEED = 1.0; // 1 rad per second
qreal MAX_ZOOM_SPEED = gradToRad(30.0); // zoom speed
qreal MIN_FOV = gradToRad(20.0);
qreal MAX_FOV = gradToRad(90.0);

qreal FISHEYE_FOV = gradToRad(180.0);

qreal MOVETO_ANIMATION_TIME = 0.5;

QnFisheyePtzController::QnFisheyePtzController(QnResource* resource):
    QnVirtualPtzController(resource),
    m_renderer(0),
    m_lastTime(0),
    m_moveToAnimation(false),
    m_spaceMapper(0),
    m_lastAR(1.0)
{
    updateSpaceMapper(DewarpingParams().viewMode, 1);
}

QnFisheyePtzController::~QnFisheyePtzController()
{
    if (m_renderer)
        m_renderer->setFisheyeController(0);
    delete m_spaceMapper;
}

void QnFisheyePtzController::updateSpaceMapper(DewarpingParams::ViewMode viewMode, int pf)
{
    if (m_spaceMapper)
        delete m_spaceMapper;

    QnScalarSpaceMapper zMapper(MIN_FOV, MAX_FOV*pf, MIN_FOV, MAX_FOV*pf, Qn::ConstantExtrapolation);
    //qreal minX, maxX, minY, maxY;
    Qn::ExtrapolationMode xExtrapolationMode;

    if (viewMode == DewarpingParams::Horizontal) {
        m_xRange = SpaceRange(gradToRad(-90.0), gradToRad(90.0));
        m_yRange = SpaceRange(gradToRad(-90.0), gradToRad(90.0));
        xExtrapolationMode = Qn::ConstantExtrapolation;
    }
    else {
        m_xRange = SpaceRange(gradToRad(-180.0), gradToRad(180.0));
        if (pf > 1)
            m_yRange = SpaceRange(0.0, gradToRad(90.0));
        else
            m_yRange = SpaceRange(0.0, gradToRad(90 + 45));
        xExtrapolationMode = Qn::PeriodicExtrapolation;
    }

    QnScalarSpaceMapper xMapper(radToGrad(m_xRange.min), radToGrad(m_xRange.max), radToGrad(m_xRange.min), radToGrad(m_xRange.max), xExtrapolationMode);
    QnScalarSpaceMapper yMapper(radToGrad(m_yRange.min), radToGrad(m_yRange.max), radToGrad(m_yRange.min), radToGrad(m_yRange.max), Qn::ConstantExtrapolation);
    m_spaceMapper = new QnPtzSpaceMapper(QnVectorSpaceMapper(xMapper, yMapper, zMapper), QStringList());
    m_spaceMapper->setFlags(Qn::FovBasedMapper);

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
    if (m_dewarpingParams.viewMode == DewarpingParams::Horizontal)
        return qBound(-xRange, value, xRange);
    else
        return value;
}

qreal QnFisheyePtzController::boundYAngle(qreal value, qreal fov, qreal aspectRatio, DewarpingParams::ViewMode viewMode) const
{
    qreal yFov = fov / aspectRatio;

    if (viewMode == DewarpingParams::Horizontal)
        return qBound(m_yRange.min + yFov/2.0, value, m_yRange.max - yFov/2.0);
    else
        //return qBound(m_yRange.min, value, m_yRange.max - yFov * 3.0/4.0);
        return qBound(m_yRange.min, value, m_yRange.max - yFov);
}

int QnFisheyePtzController::moveTo(qreal xPos, qreal yPos, qreal zoomPos)
{
    m_motion = QVector3D();

    m_dstPos.fov = qBound(MIN_FOV, zoomPos, MAX_FOV * m_dewarpingParams.panoFactor);

    m_dstPos.xAngle = boundXAngle(gradToRad(xPos), m_dstPos.fov);
    m_dstPos.yAngle = boundYAngle(gradToRad(yPos), m_dstPos.fov, m_dewarpingParams.panoFactor*m_lastAR, m_dewarpingParams.viewMode);
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
    *zoomPos = m_dewarpingParams.fov;

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

DewarpingParams QnFisheyePtzController::getDewarpingParams() const
{
    return m_dewarpingParams;
}

DewarpingParams QnFisheyePtzController::updateDewarpingParams(float ar)
{
    m_lastAR = ar;
    qint64 newTime = getUsecTimer();
    qreal timeSpend = (newTime - m_lastTime) / 1000000.0;
    DewarpingParams newParams = m_dewarpingParams;

    if (m_moveToAnimation)
    {
        if (timeSpend < MOVETO_ANIMATION_TIME && isAnimationEnabled()) 
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
        newParams.fov = qBound(MIN_FOV, newParams.fov, MAX_FOV*m_dewarpingParams.panoFactor);

        newParams.xAngle = normalizeAngle(newParams.xAngle + xSpeed * timeSpend);
        newParams.yAngle += ySpeed * timeSpend;

        m_lastTime = newTime;
    }
    newParams.xAngle = boundXAngle(newParams.xAngle, newParams.fov);
    newParams.yAngle = boundYAngle(newParams.yAngle, newParams.fov, m_dewarpingParams.panoFactor*ar, m_dewarpingParams.viewMode);
    newParams.enabled = m_dewarpingParams.enabled;

    DewarpingParams camParams = m_resource->getDewarpingParams();
    newParams.fovRot = gradToRad(camParams.fovRot);
    newParams.viewMode = camParams.viewMode;
    newParams.panoFactor = m_dewarpingParams.panoFactor;
    if (newParams.viewMode == DewarpingParams::Horizontal)
        newParams.panoFactor = qMin(newParams.panoFactor, 2.0);

    if (!(newParams == m_dewarpingParams)) 
    {
        if (newParams.viewMode != m_dewarpingParams.viewMode)
            updateSpaceMapper(newParams.viewMode, newParams.panoFactor);
        emit dewarpingParamsChanged(newParams);
        m_dewarpingParams = newParams;
    }

    //newParams.fovRot = gradToRad(-12.0); // city 360 picture
    //newParams.fovRot = gradToRad(-18.0);
    return newParams;
}

void QnFisheyePtzController::setDewarpingParams(const DewarpingParams params)
{
    if (!(m_dewarpingParams == params)) {
        setAnimationEnabled(false);

        if (params.viewMode != m_dewarpingParams.viewMode)
            updateSpaceMapper(params.viewMode, params.panoFactor);
        m_dewarpingParams = params;
        emit dewarpingParamsChanged(m_dewarpingParams);
    }
}

void QnFisheyePtzController::setEnabled(bool value)
{
    if (m_dewarpingParams.enabled != value) {
        m_dewarpingParams.enabled = value;
        emit dewarpingParamsChanged(m_dewarpingParams);
        emit spaceMapperChanged();
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
    if (m_resource->getDewarpingParams().viewMode == DewarpingParams::Horizontal) {
        qreal x = c.x() * M_PI;
        qreal y = -c.y() * M_PI;
        moveTo(radToGrad(x), radToGrad(y), fov);
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

        moveTo(radToGrad(x), radToGrad(y), fov);
    }
}

void QnFisheyePtzController::changePanoMode()
{
    m_dewarpingParams.panoFactor = m_dewarpingParams.panoFactor * 2;
    bool hView = (m_dewarpingParams.viewMode == DewarpingParams::Horizontal);
    if ((hView  && m_dewarpingParams.panoFactor > 2) ||
        (!hView && m_dewarpingParams.panoFactor > 4))
    {
        m_dewarpingParams.panoFactor = 1;
    }
    if (m_dewarpingParams.panoFactor == 1)
        m_dewarpingParams.fov = DewarpingParams().fov;
    else
        m_dewarpingParams.fov = MAX_FOV * m_dewarpingParams.panoFactor;
    emit dewarpingParamsChanged(m_dewarpingParams);
    updateSpaceMapper(m_dewarpingParams.viewMode, m_dewarpingParams.panoFactor);
}

QString QnFisheyePtzController::getPanoModeText() const
{
    return QString::number(m_dewarpingParams.panoFactor * 90);
}
