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

#include <nx/utils/log/log.h>
#include <nx/utils/log/assert.h>

using namespace nx::core;

QnFisheyePtzController::QnFisheyePtzController(QnMediaResourceWidget* widget):
    base_type(widget->resource()->toResourcePtr()),
    m_animationMode(NoAnimation),
    m_mediaDewarpingParams(widget->dewarpingParams()),
    m_itemDewarpingParams(widget->item()->dewarpingParams())
{
    m_unitSpeed = {60.0, 60.0, 0.0, 30.0};

    m_widget = widget;
    m_widget->registerAnimation(this);

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
    base_type(mediaRes->toResourcePtr()),
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

QnMediaDewarpingParams QnFisheyePtzController::mediaDewarpingParams() const
{
    return m_mediaDewarpingParams;
}

nx::vms::api::DewarpingData QnFisheyePtzController::itemDewarpingParams() const
{
    return m_itemDewarpingParams;
}

void QnFisheyePtzController::updateLimits()
{
    m_limits.minFov = 20.0;
    m_limits.maxFov = 90.0 * m_itemDewarpingParams.panoFactor;

    qreal imageAR = m_aspectRatio;
    if (m_itemDewarpingParams.panoFactor > 1)
        imageAR = 1.0 / (qreal) m_itemDewarpingParams.panoFactor;
    qreal radiusY = m_mediaDewarpingParams.radius * imageAR / m_mediaDewarpingParams.hStretch;

    qreal minY = m_mediaDewarpingParams.yCenter - radiusY;
    qreal maxY = m_mediaDewarpingParams.yCenter + radiusY;

#if 0
     // not tested yet. Also, I am not sure that it's needed for real cameras
    qreal minX = m_mediaDewarpingParams.xCenter - m_mediaDewarpingParams.radius;
    qreal maxX = m_mediaDewarpingParams.xCenter + m_mediaDewarpingParams.radius;
#endif

    if (m_mediaDewarpingParams.viewMode == QnMediaDewarpingParams::Horizontal)
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

#if 0
        // not tested yet. Also, I am not sure that it's needed for real cameras
        if (maxX > 1.0)
            m_limits.maxPan -= (maxX - 1.0) * 180.0;
        if (minX < 0.0)
            m_limits.minPan -= minX * 180.0;
#endif
    }
    else
    {
        m_unlimitedPan = true;
        m_limits.minPan = 0.0;
        m_limits.maxPan = 360.0;
        m_limits.minTilt = 0.0;
        m_limits.maxTilt = 90.0;
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
    emit changed(Qn::CapabilitiesPtzField);
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

nx::core::ptz::Vector QnFisheyePtzController::boundedPosition(const nx::core::ptz::Vector& position)
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

nx::core::ptz::Vector QnFisheyePtzController::getPositionInternal() const
{
    return nx::core::ptz::Vector(
        qRadiansToDegrees(m_itemDewarpingParams.xAngle),
        qRadiansToDegrees(m_itemDewarpingParams.yAngle),
        0.0, //< Rotation is not implemented.
        qRadiansToDegrees(m_itemDewarpingParams.fov));
}

void QnFisheyePtzController::absoluteMoveInternal(const nx::core::ptz::Vector& position)
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
        nx::core::ptz::Vector speed = m_speed * m_unitSpeed;
        absoluteMoveInternal(boundedPosition(getPositionInternal() + speed * deltaMSecs / 1000.0));
    }
    else if (m_animationMode == PositionAnimation)
    {
        m_progress += m_relativeSpeed * deltaMSecs / 1000.0;
        if (m_progress >= 1.0)
        {
            absoluteMoveInternal(m_endPosition);
            stopListening();
        }
        else
        {
            absoluteMoveInternal(boundedPosition(
                linearCombine(1.0 - m_progress, m_startPosition, m_progress, m_endPosition)));
        }
    }
}

Ptz::Capabilities QnFisheyePtzController::getCapabilities(
    const nx::core::ptz::Options& options) const
{
    if (options.type != ptz::Type::operational)
        return Ptz::NoPtzCapabilities;

    return m_capabilities;
}

bool QnFisheyePtzController::getLimits(
    Qn::PtzCoordinateSpace space,
    QnPtzLimits* limits,
    const nx::core::ptz::Options& options) const
{
    if (options.type != ptz::Type::operational)
    {
        NX_WARNING(
            this,
            lm("Getting limits - wrong PTZ type. "
                "Only operational PTZ is supported. Resource %1 (%2)")
                .args(resource()->getName(), resource()->getId()));

        return false;
    }

    if (space != Qn::LogicalPtzCoordinateSpace)
        return false;

    *limits = m_limits;

    return true;
}

bool QnFisheyePtzController::getFlip(
    Qt::Orientations* flip,
    const nx::core::ptz::Options& options) const
{
    if (options.type != ptz::Type::operational)
    {
        NX_WARNING(
            this,
            lm("Getting flip - wrong PTZ type. "
                "Only operational PTZ is supported. Resource %1 (%2)")
                .args(resource()->getName(), resource()->getId()));

        return false;
    }

    *flip = 0;
    return true;
}

bool QnFisheyePtzController::continuousMove(
    const nx::core::ptz::Vector& speed,
    const nx::core::ptz::Options& options)
{
    if (options.type != ptz::Type::operational)
    {
        NX_WARNING(
            this,
            lm("Continuous movement - wrong PTZ type. "
                "Only operational PTZ is supported. Resource %1 (%2)")
                .args(resource()->getName(), resource()->getId()));

        return false;
    }

    m_speed = speed;
    m_speed.zoom = -m_speed.zoom; /* Positive speed means that fov should decrease. */

    if (qFuzzyIsNull(speed))
    {
        stopListening();
    }
    else
    {
        m_animationMode = SpeedAnimation;
        startListening();
    }

    return true;
}

bool QnFisheyePtzController::absoluteMove(
    Qn::PtzCoordinateSpace space,
    const nx::core::ptz::Vector& position,
    qreal speed,
    const nx::core::ptz::Options& options)
{
    if (options.type != ptz::Type::operational)
    {
        NX_WARNING(
            this,
            lm("Absolute movement - wrong PTZ type. "
                "Only operational PTZ is supported. Resource %1 (%2)")
                .args(resource()->getName(), resource()->getId()));

        return false;
    }

    if (space != Qn::LogicalPtzCoordinateSpace)
        return false;

    m_speed = nx::core::ptz::Vector();
    stopListening();

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

        startListening();
    }

    return true;
}

bool QnFisheyePtzController::getPosition(
    Qn::PtzCoordinateSpace space,
    nx::core::ptz::Vector* outPosition,
    const nx::core::ptz::Options& options) const
{
    if (options.type != ptz::Type::operational)
    {
        NX_WARNING(
            this,
            lm("Getting current position - wrong PTZ type. "
                "Only operational PTZ is supported. Resource %1 (%2)")
                .args(resource()->getName(), resource()->getId()));

        return false;
    }

    if (space != Qn::LogicalPtzCoordinateSpace)
        return false;

    *outPosition = getPositionInternal();

    return true;
}

nx::core::ptz::Vector QnFisheyePtzController::positionFromRect(
    const QnMediaDewarpingParams &dewarpingParams,
    const QRectF& rect)
{
    // TODO: #PTZ
    // implement support for x/y displacement

    QPointF center = rect.center() - QPointF(0.5, 0.5);
    qreal fov = rect.width() * M_PI;

    if (dewarpingParams.viewMode == QnMediaDewarpingParams::Horizontal)
    {
        qreal x = center.x() * M_PI;
        qreal y = -center.y() * M_PI;
        return nx::core::ptz::Vector(
            qRadiansToDegrees(x),
            qRadiansToDegrees(y),
            0.0, //< Rotation is not implemented.
            qRadiansToDegrees(fov));
    }

    qreal x = -(std::atan2(center.y(), center.x()) - M_PI / 2.0);
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

    return nx::core::ptz::Vector(
        qRadiansToDegrees(x),
        qRadiansToDegrees(y),
        0.0, //< Rotation is not implemented.
        qRadiansToDegrees(fov));
}

