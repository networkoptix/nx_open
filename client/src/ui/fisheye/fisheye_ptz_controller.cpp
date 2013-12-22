#include "fisheye_ptz_controller.h"

#include <QtCore/QEasingCurve>

#include <utils/math/math.h>
#include <utils/math/linear_combination.h>

#include <core/resource/media_resource.h>
#include <core/resource/camera_resource.h>

#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/resource/resource_widget_renderer.h>

#include <ui/workbench/workbench_item.h>


// -------------------------------------------------------------------------- //
// QnFisheyePtzController
// -------------------------------------------------------------------------- //
QnFisheyePtzController::QnFisheyePtzController(QnMediaResourceWidget *widget):
    base_type(widget->resource()->toResourcePtr()),
    m_animationMode(NoAnimation)
{
    m_widget = widget;
    m_widget->registerAnimation(this);

    m_renderer = widget->renderer();
    m_renderer->setFisheyeController(this);

    connect(m_widget,           &QnResourceWidget::aspectRatioChanged,      this, &QnFisheyePtzController::updateAspectRatio);
    connect(resource(),         &QnResource::mediaDewarpingParamsChanged,   this, &QnFisheyePtzController::updateMediaDewarpingParams);
    connect(m_widget->item(),   &QnWorkbenchItem::dewarpingParamsChanged,   this, &QnFisheyePtzController::updateItemDewarpingParams);

    updateAspectRatio();
    updateItemDewarpingParams();
    updateMediaDewarpingParams();
}

QnFisheyePtzController::~QnFisheyePtzController() {
    if(m_widget)
        disconnect(m_widget, NULL, this, NULL);

    if (m_renderer)
        m_renderer->setFisheyeController(0);
}

QnMediaDewarpingParams QnFisheyePtzController::mediaDewarpingParams() const {
    return m_mediaDewarpingParams;
}

QnItemDewarpingParams QnFisheyePtzController::itemDewarpingParams() const {
    return m_itemDewarpingParams;
}

void QnFisheyePtzController::updateLimits() {
    m_limits.minFov = 20.0;
    m_limits.maxFov = 90.0 * m_itemDewarpingParams.panoFactor;

    if(m_mediaDewarpingParams.viewMode == QnMediaDewarpingParams::Horizontal) {
        m_unlimitedPan = false;
        m_limits.minPan = -90.0;
        m_limits.maxPan = 90.0;
        m_limits.minTilt = -90.0;
        m_limits.maxTilt = 90.0;
    } else {
        m_unlimitedPan = true;
        m_limits.minPan = 0.0;
        m_limits.maxPan = 360.0;
        m_limits.minTilt = 0.0;
        m_limits.maxTilt = 90.0;
    }

    absoluteMoveInternal(boundedPosition(getPositionInternal()));
}

void QnFisheyePtzController::updateCapabilities() {
    Qn::PtzCapabilities capabilities;
    if(m_mediaDewarpingParams.enabled) {
        capabilities = Qn::FisheyePtzCapabilities;
    } else {
        capabilities = Qn::NoPtzCapabilities;
    }

    if(capabilities == m_capabilities)
        return;

    m_capabilities = capabilities;
    emit capabilitiesChanged();
}

void QnFisheyePtzController::updateAspectRatio() {
    m_aspectRatio = m_widget->hasAspectRatio() ? m_widget->aspectRatio() : 1.0;
}

void QnFisheyePtzController::updateMediaDewarpingParams() {
    if (!m_widget || !m_widget->resource())
        return;

    m_mediaDewarpingParams = m_widget->resource()->getDewarpingParams();

    updateLimits();
    updateCapabilities();
    m_widget->item()->setDewarpingParams(m_itemDewarpingParams);
}

void QnFisheyePtzController::updateItemDewarpingParams() {
    int oldPanoFactor = m_itemDewarpingParams. panoFactor;
    m_itemDewarpingParams = m_widget->item()->dewarpingParams();
    int newPanoFactor = m_itemDewarpingParams.panoFactor;
    if (newPanoFactor != oldPanoFactor) {
        if (newPanoFactor > oldPanoFactor)
            m_itemDewarpingParams.fov *= newPanoFactor / oldPanoFactor;
        updateLimits();
    }
}

QVector3D QnFisheyePtzController::boundedPosition(const QVector3D &position) {
    QVector3D result = qBound(position, m_limits);

    float hFov = result.z();
    float vFov = result.z() / m_aspectRatio;

    if(!m_unlimitedPan)
        result.setX(qBound<float>(m_limits.minPan + hFov / 2.0, result.x(), m_limits.maxPan - hFov / 2.0));
    if(!m_unlimitedPan || !qFuzzyEquals(m_limits.minTilt, -90))
        result.setY(qMax(m_limits.minTilt + vFov / 2.0, result.y()));
    if(!m_unlimitedPan || !qFuzzyEquals(m_limits.maxTilt, 90))
        result.setY(qMin(m_limits.maxTilt - vFov / 2.0, result.y()));

    return result;
}

void QnFisheyePtzController::tick(int deltaMSecs) {
    if(m_animationMode == SpeedAnimation) {
        QVector3D speed = m_speed * QVector3D(60.0, 60.0, -30.0);
        absoluteMoveInternal(boundedPosition(getPositionInternal() + speed * deltaMSecs / 1000.0));
    } else if(m_animationMode == PositionAnimation) {
        m_progress += m_relativeSpeed * deltaMSecs / 1000.0;
        if(m_progress >= 1.0) {
            absoluteMoveInternal(m_endPosition);
            stopListening();
        } else {
            absoluteMoveInternal(boundedPosition(linearCombine(1.0 - m_progress, m_startPosition, m_progress, m_endPosition)));
        }
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

QVector3D QnFisheyePtzController::getPositionInternal() {
    return QVector3D(
        qRadiansToDegrees(m_itemDewarpingParams.xAngle),
        qRadiansToDegrees(m_itemDewarpingParams.yAngle),
        qRadiansToDegrees(m_itemDewarpingParams.fov)
    );
}

void QnFisheyePtzController::absoluteMoveInternal(const QVector3D &position) {
    m_itemDewarpingParams.xAngle = qDegreesToRadians(position.x());
    m_itemDewarpingParams.yAngle = qDegreesToRadians(position.y());
    m_itemDewarpingParams.fov = qDegreesToRadians(position.z());
    m_widget->item()->setDewarpingParams(m_itemDewarpingParams);
}


// -------------------------------------------------------------------------- //
// QnAbstractPtzController implementation
// -------------------------------------------------------------------------- //
Qn::PtzCapabilities QnFisheyePtzController::getCapabilities() {
    return m_capabilities;
}

bool QnFisheyePtzController::getLimits(Qn::PtzCoordinateSpace space, QnPtzLimits *limits) {
    if(space != Qn::LogicalPtzCoordinateSpace)
        return false;

    *limits = m_limits;
    return true;
}

bool QnFisheyePtzController::getFlip(Qt::Orientations *flip) {
    *flip = 0;
    return true;
}

#if 0
bool QnFisheyePtzController::getProjection(Qn::Projection *projection) {
    qreal factor = m_dewarpingParams.panoFactor;

    if(qFuzzyCompare(factor, 1.0)) {
        *projection = Qn::RectilinearProjection;
    } else if(qFuzzyCompare(factor, 2.0)) {
        *projection = Qn::Equirectangular2xProjection;
    } else if(qFuzzyCompare(factor, 4.0)) {
        *projection = Qn::Equirectangular4xProjection;
    } else {
        *projection = Qn::RectilinearProjection;
    }

    return true;
}

bool QnFisheyePtzController::setProjection(Qn::Projection projection) {
    return true; // TODO: #PTZ
    /*switch(projection) {
    case Qn::RectilinearProjection:
        m_dewarpingParams.panoFactor = 1.0;
        break;
        case Qn::
    }*/
}
#endif

bool QnFisheyePtzController::continuousMove(const QVector3D &speed) {
    m_speed = speed;

    if(qFuzzyIsNull(speed)) {
        stopListening();
    } else {
        m_animationMode = SpeedAnimation;
        startListening();
    }

    return true;
}

bool QnFisheyePtzController::absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) {
    if(space != Qn::LogicalPtzCoordinateSpace)
        return false;

    m_speed = QVector3D();
    stopListening();

    if(!qFuzzyEquals(speed, 1.0) && speed > 1.0) {
        absoluteMoveInternal(boundedPosition(position));
    } else {
        m_animationMode = PositionAnimation;
        m_startPosition = getPositionInternal();
        m_endPosition = boundedPosition(position);
        m_progress = 0.0;
        m_relativeSpeed = qBound<qreal>(0.0, speed, 1.0); // TODO: #Elric this is wrong. We need to take distance into account.
        
        startListening();
    }
    return true;
}

bool QnFisheyePtzController::getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) {
    if(space != Qn::LogicalPtzCoordinateSpace)
        return false;
    
    *position = getPositionInternal();
    return true;
}
