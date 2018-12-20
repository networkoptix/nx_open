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

    connect(m_controller, &QnAbstractPtzController::finished,
        this, &QnProxyPtzController::baseFinished);
    connect(m_controller, &QnAbstractPtzController::changed,
        this, &QnProxyPtzController::baseChanged);
}

QnPtzControllerPtr QnProxyPtzController::baseController() const
{
    return m_controller;
}

Ptz::Capabilities QnProxyPtzController::getCapabilities(const nx::core::ptz::Options& options) const
{
    return m_controller
        ? m_controller->getCapabilities(options)
        : Ptz::NoPtzCapabilities;
}

bool QnProxyPtzController::continuousMove(
    const nx::core::ptz::Vector& speed,
    const nx::core::ptz::Options& options)
{
    return m_controller
        ? m_controller->continuousMove(speed, options)
        : false;
}

bool QnProxyPtzController::continuousFocus(
    qreal speed,
    const nx::core::ptz::Options& options)
{
    return m_controller
        ? m_controller->continuousFocus(speed, options)
        : false;
}

bool QnProxyPtzController::absoluteMove(
    Qn::PtzCoordinateSpace space,
    const nx::core::ptz::Vector& position,
    qreal speed,
    const nx::core::ptz::Options& options)
{
    return m_controller
        ? m_controller->absoluteMove(space, position, speed, options)
        : false;
}

bool QnProxyPtzController::viewportMove(
    qreal aspectRatio,
    const QRectF& viewport,
    qreal speed,
    const nx::core::ptz::Options& options)
{
    return m_controller
        ? m_controller->viewportMove(aspectRatio, viewport, speed, options)
        : false;
}

bool QnProxyPtzController::relativeMove(
    const nx::core::ptz::Vector& direction,
    const nx::core::ptz::Options& options)
{
    return m_controller
        ? m_controller->relativeMove(direction, options)
        : false;
}

bool QnProxyPtzController::relativeFocus(qreal direction, const nx::core::ptz::Options& options)
{
    return m_controller
        ? m_controller->relativeFocus(direction, options)
        : false;
}

bool QnProxyPtzController::getPosition(
    Qn::PtzCoordinateSpace space,
    nx::core::ptz::Vector* position,
    const nx::core::ptz::Options& options) const
{
    if (m_controller)
    {
        return m_controller->getPosition(space, position, options);
    }
    else
    {
        NX_WARNING(this, lm("Getting current position: m_controller is nullptr."));
        return false;
    }
}

bool QnProxyPtzController::getLimits(
    Qn::PtzCoordinateSpace space,
    QnPtzLimits* limits,
    const nx::core::ptz::Options& options) const
{
    return m_controller
        ? m_controller->getLimits(space, limits, options)
        : false;
}

bool QnProxyPtzController::getFlip(
    Qt::Orientations* flip,
    const nx::core::ptz::Options& options) const
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
    const nx::core::ptz::Options& options) const
{
    return m_controller
        ? m_controller->getAuxiliaryTraits(auxiliaryTraits, options)
        : false;
}

bool QnProxyPtzController::runAuxiliaryCommand(
    const QnPtzAuxiliaryTrait& trait,
    const QString& data,
    const nx::core::ptz::Options& options)
{
    return m_controller
        ? m_controller->runAuxiliaryCommand(trait, data, options)
        : false;
}

bool QnProxyPtzController::getData(
    Qn::PtzDataFields query,
    QnPtzData* data,
    const nx::core::ptz::Options& options) const
{
    // This is important because of base implementation! Do not change it!
    return base_type::getData(query, data, options);
}

QnResourcePtr QnProxyPtzController::resource() const
{
    const auto base = baseController();
    return base ? base->resource() : QnResourcePtr();
}

void QnProxyPtzController::baseFinished(Qn::PtzCommand command, const QVariant& data)
{
    emit finished(command, data);
}

void QnProxyPtzController::baseChanged(Qn::PtzDataFields fields)
{
    emit changed(fields);
}
