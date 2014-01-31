#include "fisheye_ptz_controller.h"

#include <cassert>

#include <common/common_meta_types.h>

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

    connect(this,               &QnFisheyePtzController::finishedLater,         this, &QnAbstractPtzController::finished, Qt::QueuedConnection);
    connect(m_widget,           &QnResourceWidget::aspectRatioChanged,          this, &QnFisheyePtzController::updateAspectRatio);
    connect(m_widget,           &QnMediaResourceWidget::dewarpingParamsChanged, this, &QnFisheyePtzController::updateMediaDewarpingParams);
    connect(m_widget->item(),   &QnWorkbenchItem::dewarpingParamsChanged,       this, &QnFisheyePtzController::updateItemDewarpingParams);

    updateAspectRatio();
    updateItemDewarpingParams();
    updateMediaDewarpingParams();
}

QnFisheyePtzController::~QnFisheyePtzController() {
    /* We must be deleted from our thread because of the renderer access below. */
    assert(thread() == QThread::currentThread());

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

    qreal imageAR = m_aspectRatio / (qreal) m_itemDewarpingParams.panoFactor;
    qreal radiusY = m_mediaDewarpingParams.radius * imageAR;
    
    qreal minY = m_mediaDewarpingParams.yCenter - radiusY;
    qreal maxY = m_mediaDewarpingParams.yCenter + radiusY;

    qreal minX = m_mediaDewarpingParams.xCenter - m_mediaDewarpingParams.radius;
    qreal maxX = m_mediaDewarpingParams.xCenter + m_mediaDewarpingParams.radius;

    if(m_mediaDewarpingParams.viewMode == QnMediaDewarpingParams::Horizontal) {
        m_unlimitedPan = false;
        m_limits.minPan = -90.0;
        m_limits.maxPan = 90.0;
        
        m_limits.minTilt = -90.0;
        m_limits.maxTilt = 90.0;

        // If circle edge is out of picture, reduce maximum angle
        if (maxY > 1.0)
            m_limits.minTilt += (maxY - 1.0) * 180.0;
        if (minY < 0.0)
            m_limits.maxTilt += minY * 180.0;
        
#if 0
        // not tested yet. Also, I am not sure that it's needed for real cameras
        if (maxX > 1.0)
            m_limits.maxPan -= (maxX - 1.0) * 180.0;
        if (minX < 0.0)
            m_limits.minPan -= minX * 180.0;
#endif
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
    if (!m_widget)
        return;

    m_aspectRatio = m_widget->hasAspectRatio() ? m_widget->aspectRatio() : 1.0;
}

void QnFisheyePtzController::updateMediaDewarpingParams() {
    if (!m_widget)
        return;

    if (m_mediaDewarpingParams == m_widget->dewarpingParams())
        return;

    m_mediaDewarpingParams = m_widget->dewarpingParams();

    updateLimits();
    updateCapabilities();
}


void QnFisheyePtzController::updateItemDewarpingParams() {
    if (!m_widget)
        return;

    int oldPanoFactor = m_itemDewarpingParams. panoFactor;
    m_itemDewarpingParams = m_widget->item()->dewarpingParams();
    int newPanoFactor = m_itemDewarpingParams.panoFactor;
    if (newPanoFactor != oldPanoFactor) {
        m_itemDewarpingParams.fov *= static_cast<qreal>(newPanoFactor) / oldPanoFactor;
        updateLimits();
    }
}

QVector3D QnFisheyePtzController::boundedPosition(const QVector3D &position) {
    QVector3D result = qBound(position, m_limits);

    float hFov = result.z();
    float vFov = result.z() / m_aspectRatio;

    if(!m_unlimitedPan)
        result.setX(qBound<float>(m_limits.minPan + hFov / 2.0, result.x(), m_limits.maxPan - hFov / 2.0));
    if(!m_unlimitedPan || !qFuzzyEquals(m_limits.minTilt, -90) || m_itemDewarpingParams.panoFactor > 1)
        result.setY(qMax(m_limits.minTilt + vFov / 2.0, result.y()));
    if(!m_unlimitedPan || !qFuzzyEquals(m_limits.maxTilt, 90) || m_itemDewarpingParams.panoFactor > 1)
        result.setY(qMin(m_limits.maxTilt - vFov / 2.0, result.y()));

    return result;
}

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

    if (m_widget)
        m_widget->item()->setDewarpingParams(m_itemDewarpingParams);
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

    emit finishedLater(Qn::GetLogicalLimitsPtzCommand, QVariant::fromValue(*limits));
    return true;
}

bool QnFisheyePtzController::getFlip(Qt::Orientations *flip) {
    *flip = 0;
    emit finishedLater(Qn::GetFlipPtzCommand, QVariant::fromValue(*flip));
    return true;
}

bool QnFisheyePtzController::continuousMove(const QVector3D &speed) {
    m_speed = speed;

    if(qFuzzyIsNull(speed)) {
        stopListening();
    } else {
        m_animationMode = SpeedAnimation;
        startListening();
    }

    emit finishedLater(Qn::ContinuousMovePtzCommand, QVariant::fromValue(speed));
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

    emit finishedLater(Qn::AbsoluteLogicalMovePtzCommand, QVariant::fromValue(position));
    return true;
}

bool QnFisheyePtzController::getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) {
    if(space != Qn::LogicalPtzCoordinateSpace)
        return false;
    
    *position = getPositionInternal();

    emit finishedLater(Qn::GetLogicalPositionPtzCommand, QVariant::fromValue(*position));
    return true;
}
