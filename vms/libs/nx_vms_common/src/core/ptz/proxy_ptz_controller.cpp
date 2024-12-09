// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/utils/log/log.h>
#include "proxy_ptz_controller.h"

QnProxyPtzController::QnProxyPtzController(const QnPtzControllerPtr& controller):
    base_type(controller ? controller->resource() : QnResourcePtr())
{
    connect(this, &QnProxyPtzController::finishedLater,
        this, &QnAbstractPtzController::finished, Qt::QueuedConnection);

    setBaseController(controller);
}

void QnProxyPtzController::setBaseController(const QnPtzControllerPtr& controller)
{
    if (m_controller == controller)
        return;

    if (m_controller)
        m_controller->disconnect(this);

    m_controller = controller;
    emit baseControllerChanged();

    if (!m_controller)
        return;

    connect(m_controller.get(), &QnAbstractPtzController::finished,
        this, &QnProxyPtzController::baseFinished);
    connect(m_controller.get(), &QnAbstractPtzController::changed,
        this, &QnProxyPtzController::baseChanged);

    if (m_isInitialized)
        m_controller->initialize();
}

QnPtzControllerPtr QnProxyPtzController::baseController() const
{
    return m_controller;
}

void QnProxyPtzController::initialize()
{
    if (m_controller)
        m_controller->initialize();

    m_isInitialized = true;
}

void QnProxyPtzController::invalidate()
{
    if (m_controller)
        m_controller->invalidate();
}

Ptz::Capabilities QnProxyPtzController::getCapabilities(const Options& options) const
{
    return m_controller
        ? m_controller->getCapabilities(options)
        : Ptz::NoPtzCapabilities;
}

bool QnProxyPtzController::continuousMove(
    const Vector& speed,
    const Options& options)
{
    return m_controller
        ? m_controller->continuousMove(speed, options)
        : false;
}

bool QnProxyPtzController::continuousFocus(
    qreal speed,
    const Options& options)
{
    return m_controller
        ? m_controller->continuousFocus(speed, options)
        : false;
}

bool QnProxyPtzController::absoluteMove(
    CoordinateSpace space,
    const Vector& position,
    qreal speed,
    const Options& options)
{
    return m_controller
        ? m_controller->absoluteMove(space, position, speed, options)
        : false;
}

bool QnProxyPtzController::viewportMove(
    qreal aspectRatio,
    const QRectF& viewport,
    qreal speed,
    const Options& options)
{
    return m_controller
        ? m_controller->viewportMove(aspectRatio, viewport, speed, options)
        : false;
}

bool QnProxyPtzController::relativeMove(
    const Vector& direction,
    const Options& options)
{
    return m_controller
        ? m_controller->relativeMove(direction, options)
        : false;
}

bool QnProxyPtzController::relativeFocus(qreal direction, const Options& options)
{
    return m_controller
        ? m_controller->relativeFocus(direction, options)
        : false;
}

bool QnProxyPtzController::getPosition(
    Vector* position,
    CoordinateSpace space,
    const Options& options) const
{
    if (m_controller)
    {
        return m_controller->getPosition(position, space, options);
    }
    else
    {
        NX_WARNING(this, "Getting current position: m_controller is nullptr.");
        return false;
    }
}

bool QnProxyPtzController::getLimits(
    QnPtzLimits* limits,
    CoordinateSpace space,
    const Options& options) const
{
    return m_controller
        ? m_controller->getLimits(limits, space, options)
        : false;
}

bool QnProxyPtzController::getFlip(
    Qt::Orientations* flip,
    const Options& options) const
{
    return m_controller
        ? m_controller->getFlip(flip, options)
        : false;
}

bool QnProxyPtzController::createPreset(const QnPtzPreset& preset)
{
    return m_controller
        ? m_controller->createPreset(preset)
        : false;
}

bool QnProxyPtzController::updatePreset(const QnPtzPreset& preset)
{
    return m_controller
        ? m_controller->updatePreset(preset)
        : false;
}

bool QnProxyPtzController::removePreset(const QString& presetId)
{
    return m_controller
        ? m_controller->removePreset(presetId)
        : false;
}

bool QnProxyPtzController::activatePreset(const QString& presetId, qreal speed)
{
    return m_controller
        ? m_controller->activatePreset(presetId, speed)
        : false;
}

bool QnProxyPtzController::QnProxyPtzController::getPresets(QnPtzPresetList* presets) const
{
    return m_controller
        ? m_controller->getPresets(presets)
        : false;
}

bool QnProxyPtzController::createTour(const QnPtzTour& tour)
{
    return m_controller
        ? m_controller->createTour(tour)
        : false;
}

bool QnProxyPtzController::removeTour(const QString& tourId)
{
    return m_controller
        ? m_controller->removeTour(tourId)
        : false;
}

bool QnProxyPtzController::activateTour(const QString& tourId)
{
    return m_controller
        ? m_controller->activateTour(tourId)
        : false;
}

std::optional<QnPtzTour> QnProxyPtzController::getActiveTour()
{
    return m_controller
        ? m_controller->getActiveTour()
        : std::nullopt;
}

bool QnProxyPtzController::getTours(
    QnPtzTourList* tours) const
{
    return m_controller
        ? m_controller->getTours(tours)
        : false;
}

bool QnProxyPtzController::getActiveObject(QnPtzObject* activeObject) const
{
    return m_controller
        ? m_controller->getActiveObject(activeObject)
        : false;
}

bool QnProxyPtzController::updateHomeObject(const QnPtzObject& homeObject)
{
    return m_controller
        ? m_controller->updateHomeObject(homeObject)
        : false;
}

bool QnProxyPtzController::getHomeObject(QnPtzObject* homeObject) const
{
    return m_controller
        ? m_controller->getHomeObject(homeObject)
        : false;
}

bool QnProxyPtzController::getAuxiliaryTraits(
    QnPtzAuxiliaryTraitList* auxiliaryTraits,
    const Options& options) const
{
    return m_controller
        ? m_controller->getAuxiliaryTraits(auxiliaryTraits, options)
        : false;
}

bool QnProxyPtzController::runAuxiliaryCommand(
    const QnPtzAuxiliaryTrait& trait,
    const QString& data,
    const Options& options)
{
    return m_controller
        ? m_controller->runAuxiliaryCommand(trait, data, options)
        : false;
}

bool QnProxyPtzController::getData(
    QnPtzData* data,
    DataFields query,
    const Options& options) const
{
    // This is important because of base implementation! Do not change it!
    return base_type::getData(data, query, options);
}

QnResourcePtr QnProxyPtzController::resource() const
{
    const auto base = baseController();
    return base ? base->resource() : QnResourcePtr();
}

void QnProxyPtzController::baseFinished(Command command, const QVariant& data)
{
    emit finished(command, data);
}

void QnProxyPtzController::baseChanged(DataFields fields)
{
    emit changed(fields);
}
