#include "fisheye_ptz_controller.h"

#include <QtCore/QEasingCurve>

#include <utils/math/math.h>

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
    m_animating(false)
{
    m_widget = widget;

    m_resource = widget->resource();

    m_renderer = widget->renderer();
    m_renderer->setFisheyeController(this);

    m_timer.start();

    connect(m_widget, SIGNAL(aspectRatioChanged()), this, SLOT(updateAspectRatio()));
    connect(m_widget.data(),            &QnMediaResourceWidget::itemDewarpingParamsChanged,     this, &QnFisheyePtzController::updateItemDewarpingParams);
    connect(m_widget->camera().data(),  &QnVirtualCameraResource::mediaDewarpingParamsChanged,  this, &QnFisheyePtzController::updateMediaDewarpingParams);

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
        if(m_mediaDewarpingParams.viewMode == QnMediaDewarpingParams::VerticalUp) {
            m_limits.minTilt = 0.0;
            m_limits.maxTilt = 90.0;
        } else {
            m_limits.minTilt = -90.0;
            m_limits.maxTilt = 0.0;
        }
    }
}

void QnFisheyePtzController::updateCapabilities() {
    Qn::PtzCapabilities capabilities;
    if(m_mediaDewarpingParams.enabled) {
        capabilities = Qn::FisheyePtzCapabilities;
//            Qn::ContinuousPtzCapabilities | Qn::AbsolutePtzCapabilities |
//            Qn::FlipPtzCapability | Qn::LimitsPtzCapability |
//            Qn::LogicalPositioningPtzCapability |
//            Qn::VirtualPtzCapability; //TODO: #GDM PTZ compare with QnMediaResource's method
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
    m_mediaDewarpingParams = m_widget->resource()->getDewarpingParams();

    updateLimits();
    updateCapabilities();
}

void QnFisheyePtzController::updateItemDewarpingParams() {
    m_itemDewarpingParams = m_widget->itemDewarpingParams();

    updateLimits();
}

QVector3D QnFisheyePtzController::boundedPosition(const QVector3D &position) {
    QVector3D result = qBound(position, m_limits);

    float hFov = result.z();
    float vFov = result.z() / m_aspectRatio;

    if (!m_unlimitedPan)
        result.setX(qBound<float>(m_limits.minPan + hFov / 2.0, result.x(), m_limits.maxPan - hFov / 2.0));
    result.setY(qBound<float>(m_limits.minTilt + vFov / 2.0, result.y(), m_limits.maxTilt - vFov / 2.0));

    // Note: For non-horizontal view mode it was: return qBound(m_yRange.min, value, m_yRange.max - yFov);

    return result;
}

void QnFisheyePtzController::tick() {
    if(!m_animating)
        return;

    qint64 elapsed = m_timer.restart();
    QVector3D speed = m_speed * QVector3D(60.0, 60.0, -30.0);
    absoluteMoveInternal(boundedPosition(getPositionInternal() + speed * elapsed / 1000.0));
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
        m_animating = false;
    } else {
        m_animating = true;
        m_timer.restart();
    }

    return true;
}

bool QnFisheyePtzController::absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) {
    if(space != Qn::LogicalPtzCoordinateSpace)
        return false;

    m_speed = QVector3D();
    m_animating = false;

    absoluteMoveInternal(boundedPosition(position));
    return true;
}

bool QnFisheyePtzController::getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) {
    if(space != Qn::LogicalPtzCoordinateSpace)
        return false;
    
    *position = getPositionInternal();
    return true;
}




