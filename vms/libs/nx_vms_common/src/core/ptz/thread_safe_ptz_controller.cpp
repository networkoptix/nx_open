// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "thread_safe_ptz_controller.h"

namespace nx::vms::common::ptz {

ThreadSafePtzController::ThreadSafePtzController(QnPtzControllerPtr controller):
	base_type(controller)
{
}

Ptz::Capabilities ThreadSafePtzController::getCapabilities(const Options& options) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return base_type::getCapabilities(options);
}

bool ThreadSafePtzController::continuousMove(const Vector& speed, const Options& options)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return base_type::continuousMove(speed, options);
}

bool ThreadSafePtzController::continuousFocus(qreal speed, const Options& options)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return base_type::continuousFocus(speed, options);
}

bool ThreadSafePtzController::absoluteMove(
    CoordinateSpace space,
    const Vector& position,
    qreal speed,
    const Options& options)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return base_type::absoluteMove(space, position, speed, options);
}

bool ThreadSafePtzController::viewportMove(
    qreal aspectRatio,
    const QRectF& viewport,
    qreal speed,
    const Options& options)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return base_type::viewportMove(aspectRatio, viewport, speed, options);
}

bool ThreadSafePtzController::relativeMove(
    const Vector& direction,
    const Options& options)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return base_type::relativeMove(direction, options);
}

bool ThreadSafePtzController::relativeFocus(
    qreal direction,
    const Options& options)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return base_type::relativeFocus(direction, options);
}

bool ThreadSafePtzController::getPosition(
    Vector* outPosition,
    CoordinateSpace space,
    const Options& options) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return base_type::getPosition(outPosition, space, options);
}

bool ThreadSafePtzController::getLimits(
    QnPtzLimits* limits,
    CoordinateSpace space,
    const Options& options) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return base_type::getLimits(limits, space, options);
}

bool ThreadSafePtzController::getFlip(
    Qt::Orientations* flip,
    const Options& options) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return base_type::getFlip(flip, options);
}

bool ThreadSafePtzController::createPreset(const QnPtzPreset& preset)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return base_type::createPreset(preset);
}

bool ThreadSafePtzController::updatePreset(const QnPtzPreset& preset)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return base_type::updatePreset(preset);
}

bool ThreadSafePtzController::removePreset(const QString& presetId)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return base_type::removePreset(presetId);
}

bool ThreadSafePtzController::activatePreset(const QString& presetId, qreal speed)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return base_type::activatePreset(presetId, speed);
}

bool ThreadSafePtzController::getPresets(QnPtzPresetList* presets) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return base_type::getPresets(presets);
}

bool ThreadSafePtzController::createTour(const QnPtzTour& tour)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return base_type::createTour(tour);
}

bool ThreadSafePtzController::removeTour(const QString& tourId)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return base_type::removeTour(tourId);
}

bool ThreadSafePtzController::activateTour(const QString& tourId)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return base_type::activateTour(tourId);
}

bool ThreadSafePtzController::getTours(QnPtzTourList* tours) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return base_type::getTours(tours);
}

bool ThreadSafePtzController::getActiveObject(QnPtzObject* activeObject) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return base_type::getActiveObject(activeObject);
}

bool ThreadSafePtzController::updateHomeObject(const QnPtzObject& homeObject)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return base_type::updateHomeObject(homeObject);
}

bool ThreadSafePtzController::getHomeObject(QnPtzObject* homeObject) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return base_type::getHomeObject(homeObject);
}

bool ThreadSafePtzController::getAuxiliaryTraits(
    QnPtzAuxiliaryTraitList* auxiliaryTraits,
    const Options& options) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return base_type::getAuxiliaryTraits(auxiliaryTraits, options);
}

bool ThreadSafePtzController::runAuxiliaryCommand(
    const QnPtzAuxiliaryTrait& trait,
    const QString& data,
    const Options& options)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return base_type::runAuxiliaryCommand(trait, data, options);
}

bool ThreadSafePtzController::getData(
    QnPtzData* data,
    DataFields query,
    const Options& options) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return base_type::getData(data, query, options);
}

} // namespace nx::vms::common::ptz
