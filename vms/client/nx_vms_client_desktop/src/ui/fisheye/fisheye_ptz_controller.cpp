// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "fisheye_ptz_controller.h"

#include <cassert>

#include <QtCore/QThread>

#include <common/common_meta_types.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_resource.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/math/math.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/resource/resource_widget_renderer.h>
#include <ui/workbench/workbench_item.h>
#include <utils/math/linear_combination.h>

using namespace nx::core;
using namespace nx::vms::api;
using namespace nx::vms::common::ptz;

QnFisheyePtzController::QnFisheyePtzController(QnMediaResourceWidget* widget):
    base_type(widget->resource()),
    m_animationMode(NoAnimation),
    m_mediaDewarpingParams(widget->dewarpingParams()),
    m_itemDewarpingParams(widget->item()->dewarpingParams())
{
    connect(m_animationTimerListener.get(), &AnimationTimerListener::tick, this,
        &QnFisheyePtzController::tick);

    m_unitSpeed = {60.0, 60.0, 0.0, 30.0};

    m_widget = widget;
    m_widget->registerAnimation(m_animationTimerListener);

    m_renderer = widget->renderer();
    m_renderer->setFisheyeController(this);

    connect(m_widget, &QnResourceWidget::aspectRatioChanged,
        this, &QnFisheyePtzController::updateAspectRatio);
    connect(m_widget, &QnMediaResourceWidget::dewarpingParamsChanged,
        this, &QnFisheyePtzController::updateMediaDewarpingParams);
    connect(m_widget->item(), &QnWorkbenchItem::dewarpingParamsChanged,
        this, &QnFisheyePtzController::updateItemDewarpingParams);

    updateAspectRatio();
    updateCapabilities();
    updateLimits();
}

QnFisheyePtzController::QnFisheyePtzController(const QnMediaResourcePtr& mediaRes):
    base_type(mediaRes),
    m_animationMode(NoAnimation),
    m_mediaDewarpingParams(mediaRes->getDewarpingParams())
{
    updateCapabilities();
    updateLimits();
}

QnAspectRatio QnFisheyePtzController::customAR() const
{
    return m_widget->resource()->customAspectRatio();
}

QnFisheyePtzController::~QnFisheyePtzController()
{
    /* We must be deleted from our thread because of the renderer access below. */
    NX_ASSERT(thread() == QThread::currentThread());

    if (m_renderer)
        m_renderer->setFisheyeController(0);
}

nx::vms::api::dewarping::MediaData QnFisheyePtzController::mediaDewarpingParams() const
{
    return m_mediaDewarpingParams;
}

nx::vms::api::dewarping::ViewData QnFisheyePtzController::itemDewarpingParams() const
{
    return m_itemDewarpingParams;
}

void QnFisheyePtzController::updateLimits()
{
    m_limits.minFov = 20.0;
    m_limits.maxFov = 90.0 * m_itemDewarpingParams.panoFactor;

    if (m_mediaDewarpingParams.is360VR())
    {
        m_unlimitedPan = true;
        m_limits.minPan = 0.0;
        m_limits.maxPan = 360.0;
        m_limits.minTilt = -90.0;
        m_limits.maxTilt = 90.0;
    }
    else
    {
        qreal imageAR = m_aspectRatio;
        if (m_itemDewarpingParams.panoFactor > 1)
            imageAR = 1.0 / (qreal)m_itemDewarpingParams.panoFactor;
        qreal radiusY = m_mediaDewarpingParams.radius * imageAR / m_mediaDewarpingParams.hStretch;

        qreal minY = m_mediaDewarpingParams.yCenter - radiusY;
        qreal maxY = m_mediaDewarpingParams.yCenter + radiusY;

        if (m_mediaDewarpingParams.viewMode == dewarping::FisheyeCameraMount::wall)
        {
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
        }
        else
        {
            m_unlimitedPan = true;
            m_limits.minPan = 0.0;
            m_limits.maxPan = 360.0;
            m_limits.minTilt = 0.0;
            m_limits.maxTilt = 90.0;
        }
    }

    if (m_capabilities)
        absoluteMoveInternal(boundedPosition(getPositionInternal()));
}

void QnFisheyePtzController::updateCapabilities()
{
    Ptz::Capabilities capabilities = Ptz::NoPtzCapabilities;
    if (m_mediaDewarpingParams.enabled)
    {
        capabilities = Ptz::ContinuousPtzCapabilities | Ptz::AbsolutePtzCapabilities
            | Ptz::LogicalPositioningPtzCapability | Ptz::VirtualPtzCapability;
    }

    if (capabilities == m_capabilities)
        return;

    m_capabilities = capabilities;
    emit changed(DataField::capabilities);
}

void QnFisheyePtzController::updateAspectRatio()
{
    if (!m_widget)
        return;

    m_aspectRatio = m_widget->hasAspectRatio() ? m_widget->aspectRatio() : 1.0;
}

void QnFisheyePtzController::updateMediaDewarpingParams()
{
    if (!m_widget)
        return;

    if (m_mediaDewarpingParams == m_widget->dewarpingParams())
        return;

    m_mediaDewarpingParams = m_widget->dewarpingParams();

    updateLimits();
    updateCapabilities();
}


void QnFisheyePtzController::updateItemDewarpingParams()
{
    if (!m_widget || !m_widget->item())
        return;

    int oldPanoFactor = m_itemDewarpingParams. panoFactor;
    m_itemDewarpingParams = m_widget->item()->dewarpingParams();
    int newPanoFactor = m_itemDewarpingParams.panoFactor;
    if (newPanoFactor != oldPanoFactor)
    {
        m_itemDewarpingParams.fov *= static_cast<qreal>(newPanoFactor) / oldPanoFactor;
        updateLimits();
    }
}

Vector QnFisheyePtzController::boundedPosition(const Vector& position)
{
    auto result = qBound(position, m_limits);

    const auto hFov = result.zoom;
    const auto vFov = result.zoom / m_aspectRatio;

    if (!m_unlimitedPan)
    {
        result.pan = qBound<double>(
            m_limits.minPan + hFov / 2.0, result.pan, m_limits.maxPan - hFov / 2.0);
    }

    if (!m_unlimitedPan || !qFuzzyEquals(m_limits.minTilt, -90)
        || m_itemDewarpingParams.panoFactor > 1)
    {
        result.tilt = qMax(m_limits.minTilt + vFov / 2.0, result.tilt);
    }

    if (!m_unlimitedPan || !qFuzzyEquals(m_limits.maxTilt, 90)
        || m_itemDewarpingParams.panoFactor > 1)
    {
        result.tilt = qMin(m_limits.maxTilt - vFov  / 2.0, result.tilt);
    }

    return result;
}

Vector QnFisheyePtzController::getPositionInternal() const
{
    return Vector(
        qRadiansToDegrees(m_itemDewarpingParams.xAngle),
        qRadiansToDegrees(m_itemDewarpingParams.yAngle),
        0.0, //< Rotation is not implemented.
        qRadiansToDegrees(m_itemDewarpingParams.fov));
}

void QnFisheyePtzController::absoluteMoveInternal(const Vector& position)
{
    m_itemDewarpingParams.xAngle = qDegreesToRadians(position.pan);
    m_itemDewarpingParams.yAngle = qDegreesToRadians(position.tilt);
    m_itemDewarpingParams.fov = qDegreesToRadians(position.zoom);

    /* We check for item as we can get here in a rare case when item is
     * destroyed, but the widget is not (yet). */
    if (m_widget && m_widget->item())
        m_widget->item()->setDewarpingParams(m_itemDewarpingParams);
}

void QnFisheyePtzController::tick(int deltaMSecs)
{
    if (m_animationMode == SpeedAnimation)
    {
        Vector speed = m_speed * m_unitSpeed;
        absoluteMoveInternal(boundedPosition(getPositionInternal() + speed * deltaMSecs / 1000.0));
    }
    else if (m_animationMode == PositionAnimation)
    {
        m_progress += m_relativeSpeed * deltaMSecs / 1000.0;
        if (m_progress >= 1.0)
        {
            absoluteMoveInternal(m_endPosition);
            m_animationTimerListener->stopListening();
        }
        else
        {
            absoluteMoveInternal(boundedPosition(
                linearCombine(1.0 - m_progress, m_startPosition, m_progress, m_endPosition)));
        }
    }
}

Ptz::Capabilities QnFisheyePtzController::getCapabilities(
    const Options& options) const
{
    if (options.type != Type::operational)
        return Ptz::NoPtzCapabilities;

    return m_capabilities;
}

bool QnFisheyePtzController::getLimits(
    QnPtzLimits* limits,
    CoordinateSpace space,
    const Options& options) const
{
    if (options.type != Type::operational)
    {
        NX_WARNING(
            this,
            "Getting limits - wrong PTZ type. Only operational PTZ is supported. Resource %1",
            resource());

        return false;
    }

    if (space != CoordinateSpace::logical)
        return false;

    *limits = m_limits;

    return true;
}

bool QnFisheyePtzController::getFlip(
    Qt::Orientations* flip,
    const Options& options) const
{
    if (options.type != Type::operational)
    {
        NX_WARNING(
            this,
            nx::format("Getting flip - wrong PTZ type. "
                "Only operational PTZ is supported. Resource %1 (%2)")
                .args(resource()->getName(), resource()->getId()));

        return false;
    }

    *flip = {};
    return true;
}

bool QnFisheyePtzController::continuousMove(
    const Vector& speed,
    const Options& options)
{
    if (options.type != Type::operational)
    {
        NX_WARNING(
            this,
            nx::format("Continuous movement - wrong PTZ type. "
                "Only operational PTZ is supported. Resource %1 (%2)")
                .args(resource()->getName(), resource()->getId()));

        return false;
    }

    m_speed = speed;
    m_speed.zoom = -m_speed.zoom; /* Positive speed means that fov should decrease. */

    if (qFuzzyIsNull(speed))
    {
        m_animationTimerListener->stopListening();
    }
    else
    {
        m_animationMode = SpeedAnimation;
        m_animationTimerListener->startListening();
    }

    return true;
}

bool QnFisheyePtzController::absoluteMove(
    CoordinateSpace space,
    const Vector& position,
    qreal speed,
    const Options& options)
{
    if (options.type != Type::operational)
    {
        NX_WARNING(
            this,
            nx::format("Absolute movement - wrong PTZ type. "
                "Only operational PTZ is supported. Resource %1 (%2)")
                .args(resource()->getName(), resource()->getId()));

        return false;
    }

    if (space != CoordinateSpace::logical)
        return false;

    m_speed = Vector();
    m_animationTimerListener->stopListening();

    if (!qFuzzyEquals(speed, 1.0) && speed > 1.0)
    {
        absoluteMoveInternal(boundedPosition(position));
    }
    else
    {
        m_animationMode = PositionAnimation;
        m_startPosition = getPositionInternal();
        m_endPosition = boundedPosition(position);

        /* Try to place start & end closer to each other */
        if (m_unlimitedPan)
        {
            m_startPosition.pan = qMod(m_startPosition.pan, 360.0);
            m_endPosition.pan = qMod(m_endPosition.pan, 360.0);

            if (m_endPosition.pan - 180.0 > m_startPosition.pan)
                m_endPosition.pan = m_endPosition.pan - 360.0;
            if (m_endPosition.pan + 180.0 < m_startPosition.pan)
                m_endPosition.pan = m_endPosition.pan + 360.0;
        }

        m_progress = 0.0;

        const auto distance = m_endPosition - m_startPosition;

        const qreal panTiltTime = QVector2D(
            distance.pan / m_unitSpeed.pan,
            distance.tilt / m_unitSpeed.tilt).length();

        const qreal zoomTime = distance.zoom / m_unitSpeed.zoom;
        m_relativeSpeed = qBound(0.0, speed, 1.0) / qMax(panTiltTime, zoomTime);

        m_animationTimerListener->startListening();
    }

    return true;
}

bool QnFisheyePtzController::getPosition(
    Vector* outPosition,
    CoordinateSpace space,
    const Options& options) const
{
    if (options.type != Type::operational)
    {
        NX_WARNING(
            this,
            nx::format("Getting current position - wrong PTZ type. "
                "Only operational PTZ is supported. Resource %1 (%2)")
                .args(resource()->getName(), resource()->getId()));

        return false;
    }

    if (space != CoordinateSpace::logical)
        return false;

    *outPosition = getPositionInternal();

    return true;
}

Vector QnFisheyePtzController::positionFromRect(
    const nx::vms::api::dewarping::MediaData& dewarpingParams,
    const QRectF& rect)
{
    // TODO: #vkutin Implement support for x/y displacement.

    const QPointF center = rect.center() - QPointF(0.5, 0.5);
    const qreal fov = rect.width() * M_PI;

    if (dewarpingParams.is360VR())
    {
        const QPointF angles(center.x() * M_PI * 2.0, -center.y() * M_PI);
        if (qFuzzyIsNull(dewarpingParams.sphereBeta))
        {
            return Vector(
                qRadiansToDegrees(angles.x()),
                qRadiansToDegrees(angles.y()),
                0.0, //< Rotation is not implemented.
                qRadiansToDegrees(fov));
        }

        // Horizon correction.

        QMatrix4x4 correctionMatrix;
        correctionMatrix.rotate(-dewarpingParams.sphereAlpha, 0, 0, 1);
        correctionMatrix.rotate(-dewarpingParams.sphereBeta, 1, 0, 0);
        correctionMatrix.rotate(dewarpingParams.sphereAlpha, 0, 0, 1);

        const qreal cosTheta = cos(angles.x());
        const qreal sinTheta = sin(angles.x());
        const qreal cosPhi = cos(angles.y());
        const qreal sinPhi = sin(angles.y());
        const QVector3D pointOnSphere(cosPhi * sinTheta, cosPhi * cosTheta, sinPhi);
        const QVector3D corrected = correctionMatrix * pointOnSphere;
        const QPointF correctedAngles(atan2(corrected.x(), corrected.y()), asin(corrected.z()));

        return Vector(
            qRadiansToDegrees(correctedAngles.x()),
            qRadiansToDegrees(correctedAngles.y()),
            0.0, //< Rotation is not implemented.
            qRadiansToDegrees(fov));
    }

    if (dewarpingParams.viewMode == dewarping::FisheyeCameraMount::wall)
    {
        const QPointF rotatedCenter = QTransform().rotate(-dewarpingParams.fovRot) * center;
        const qreal x = rotatedCenter.x() * M_PI;
        const qreal y = -rotatedCenter.y() * M_PI;

        return Vector(
            qRadiansToDegrees(x),
            qRadiansToDegrees(y),
            0.0, //< Rotation is not implemented.
            qRadiansToDegrees(fov));
    }

    const qreal x = -(std::atan2(center.y(), center.x()) + M_PI / 2.0);
    qreal y = 0;

    if (qAbs(center.x()) > qAbs(center.y()))
    {
        y = center.x() > 0
            ? (1.0 - rect.right()) *  M_PI
            : rect.left() * M_PI;
    }
    else
    {
        y = center.y() > 0
            ? (1.0 - rect.bottom()) * M_PI
            : rect.top() * M_PI;
    }

    return Vector(
        qRadiansToDegrees(x) + dewarpingParams.fovRot,
        qRadiansToDegrees(y),
        0.0, //< Rotation is not implemented.
        qRadiansToDegrees(fov));
}
