#include "fisheye_ptz_controller.h"

#include <QtCore/QEasingCurve>

#include <utils/math/math.h>

#include <core/resource/media_resource.h>

#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/resource/resource_widget_renderer.h>


// -------------------------------------------------------------------------- //
// QnFisheyePtzController
// -------------------------------------------------------------------------- //
QnFisheyePtzController::QnFisheyePtzController(QnMediaResourceWidget *widget):
    base_type(widget->resource()->toResourcePtr()),
    m_animation(NoAnimation)
{
    m_widget = widget;

    m_resource = widget->resource();

    m_renderer = widget->renderer();
    m_renderer->setFisheyeController(this);

    m_timer.start();

    connect(m_widget, SIGNAL(aspectRatioChanged()), this, SLOT(at_widget_aspectRatioChanged()));
    connect(m_widget, SIGNAL(dewarpingParamsChanged()), this, SLOT(at_widget_dewarpingParamsChanged()));

    updateLimits();
}

QnFisheyePtzController::~QnFisheyePtzController() {
    if(m_widget)
        disconnect(m_widget, NULL, this, NULL);

    if (m_renderer)
        m_renderer->setFisheyeController(0);
}

const DewarpingParams &QnFisheyePtzController::dewarpingParams() const {
    return m_dewarpingParams;
}

/*void QnFisheyePtzController::setDewarpingParams(const DewarpingParams &dewarpingParams) {
    m_dewarpingParams = dewarpingParams;

    updateLimits();
}*/

void QnFisheyePtzController::updateLimits() {
    m_limits.minFov = 20.0;
    m_limits.maxFov = 90.0 * m_dewarpingParams.panoFactor;

    if(m_dewarpingParams.viewMode == DewarpingParams::Horizontal) {
        m_unlimitedPan = false;
        m_limits.minPan = -90.0;
        m_limits.maxPan = 90.0;
        m_limits.minTilt = -90.0;
        m_limits.maxTilt = 90.0;
    } else {
        m_unlimitedPan = true;
        m_limits.minPan = -36000.0;
        m_limits.maxPan = 36000.0;
        if(m_dewarpingParams.viewMode == DewarpingParams::VerticalUp) {
            m_limits.minTilt = 0.0;
            m_limits.maxTilt = 90.0;
        } else {
            m_limits.minTilt = -90.0;
            m_limits.maxTilt = 0.0;
        }
    }
}

QVector3D QnFisheyePtzController::boundedPosition(const QVector3D &position) {
    QVector3D result = qBound(position, m_limits);

    float hFov = result.z();
    float vFov = result.z() / m_aspectRatio;

    if (!m_unlimitedPan)
        result.setX(qBound<float>(m_limits.minPan + hFov / 2.0, result.x(), m_limits.maxPan - hFov / 2.0));
    result.setY(qBound<float>(m_limits.minPan + vFov / 2.0, result.y(), m_limits.maxPan - vFov / 2.0));

    // Note: For non-horizontal view mode it was: return qBound(m_yRange.min, value, m_yRange.max - yFov);

    return result;
}

void QnFisheyePtzController::tick() {
    qint64 elapsed = m_timer.restart(); // TODO: #Elric value is not used when there is no animation => don't start the timer.
    
    switch (m_animation) {
    case AbsoluteMoveAnimation: {
        break; // TODO
    }
    case ContinuousMoveAnimation: {
        QVector3D speed = m_speed * QVector3D(30.0, 60.0, 60.0);

        QVector3D oldPosition;
        getPosition(Qn::LogicalCoordinateSpace, &oldPosition);

        QVector3D newPosition = boundedPosition(oldPosition + speed * elapsed / 1000.0);

        m_dewarpingParams.xAngle = qDegreesToRadians(newPosition.x());
        m_dewarpingParams.yAngle = qDegreesToRadians(newPosition.y());
        m_dewarpingParams.fov = qDegreesToRadians(newPosition.z());
        break;
    }
    case NoAnimation:
    default:
        break;
    }

}

#if 0
DewarpingParams QnFisheyePtzController::updateDewarpingParams(float ar) {
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
        qreal zoomSpeed = -m_speed.z() * MAX_ZOOM_SPEED;
        qreal xSpeed = m_speed.x() * MAX_MOVE_SPEED;
        qreal ySpeed = m_speed.y() * MAX_MOVE_SPEED;

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
    newParams.fovRot = qDegreesToRadians(camParams.fovRot);
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

void QnFisheyePtzController::setDewarpingParams(const DewarpingParams params) {
    if (!(m_dewarpingParams == params)) {
        setAnimationEnabled(false);

        if (params.viewMode != m_dewarpingParams.viewMode)
            updateSpaceMapper(params.viewMode, params.panoFactor);
        m_dewarpingParams = params;
        emit dewarpingParamsChanged(m_dewarpingParams);
    }
}

/*void QnFisheyePtzController::changePanoMode() {
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

QString QnFisheyePtzController::getPanoModeText() const {
    return QString::number(m_dewarpingParams.panoFactor * 90);
}*/
#endif

void QnFisheyePtzController::updateCapabilities() {
    Qn::PtzCapabilities capabilities;
    if(m_dewarpingParams.enabled) {
        capabilities = Qn::ContinuousPtzCapabilities | Qn::AbsolutePtzCapabilities | Qn::LogicalPositioningPtzCapability | Qn::LimitsPtzCapability;
    } else {
        capabilities = Qn::NoPtzCapabilities;
    }

    if(capabilities == m_capabilities)
        return;

    m_capabilities = capabilities;
    emit capabilitiesChanged();
}


// -------------------------------------------------------------------------- //
// QnAbstractPtzController implementation
// -------------------------------------------------------------------------- //
Qn::PtzCapabilities QnFisheyePtzController::getCapabilities() {
    return m_capabilities;
}

bool QnFisheyePtzController::getLimits(Qn::PtzCoordinateSpace space, QnPtzLimits *limits) {
    if(space != Qn::LogicalCoordinateSpace)
        return false;

    *limits = m_limits;
    return true;
}

bool QnFisheyePtzController::getFlip(Qt::Orientations *flip) {
    *flip = 0;
    return true;
}

bool QnFisheyePtzController::continuousMove(const QVector3D &speed) {
    m_speed = speed;

    if(qFuzzyIsNull(speed)) {
        m_animation = NoAnimation;
    } else {
        m_animation = ContinuousMoveAnimation;
    }

    return true;
}

bool QnFisheyePtzController::absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position) {
    if(space != Qn::LogicalCoordinateSpace)
        return false;

    m_speed = QVector3D();

    m_targetPosition = boundedPosition(position);
    getPosition(Qn::LogicalCoordinateSpace, &m_sourcePosition);

    /* Bring pan positions close to each other. */
    if(m_unlimitedPan) {
        m_targetPosition.setX(qMod(m_targetPosition.x(), 360.0));
        m_sourcePosition.setX(qMod(m_targetPosition.y(), 360.0));

        if(m_targetPosition.x() - m_sourcePosition.x() > 180.0) {
            m_targetPosition.setX(m_targetPosition.x() - 360.0);
        } else if(m_targetPosition.x() - m_sourcePosition.x() < -180.0) {
            m_sourcePosition.setX(m_sourcePosition.x() - 360.0);
        }
    }

    m_animation = AbsoluteMoveAnimation;
    return true;
}

bool QnFisheyePtzController::getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) {
    if(space != Qn::LogicalCoordinateSpace)
        return false;

    position->setX(qRadiansToDegrees(m_dewarpingParams.xAngle));
    position->setY(qRadiansToDegrees(m_dewarpingParams.yAngle));
    position->setZ(qRadiansToDegrees(m_dewarpingParams.fov));
    return true;
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnFisheyePtzController::at_widget_aspectRatioChanged() {
    m_aspectRatio = m_widget->hasAspectRatio() ? m_widget->aspectRatio() : 1.0;
}

void QnFisheyePtzController::at_widget_dewarpingParamsChanged() {
    m_dewarpingParams = m_widget->dewarpingParams();

    updateLimits();
    updateCapabilities();
}



