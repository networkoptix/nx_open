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

Ptz::Capabilities QnProxyPtzController::getCapabilities() const
{
    return m_controller
        ? m_controller->getCapabilities()
        : Ptz::NoPtzCapabilities;
}

bool QnProxyPtzController::continuousMove(const QVector3D& speed)
{
    return m_controller
        ? m_controller->continuousMove(speed)
        : false;
}

bool QnProxyPtzController::continuousFocus(qreal speed)
{
    return m_controller
        ? m_controller->continuousFocus(speed)
        : false;
}

bool QnProxyPtzController::absoluteMove(
    Qn::PtzCoordinateSpace space,
    const QVector3D& position,
    qreal speed)
{
    return m_controller
        ? m_controller->absoluteMove(space, position, speed)
        : false;
}

bool QnProxyPtzController::viewportMove(
    qreal aspectRatio,
    const QRectF& viewport,
    qreal speed)
{
    return m_controller
        ? m_controller->viewportMove(aspectRatio, viewport, speed)
        : false;
}

bool QnProxyPtzController::getPosition(
    Qn::PtzCoordinateSpace space,
    QVector3D* position) const
{
    return m_controller
        ? m_controller->getPosition(space, position)
        : false;
}

bool QnProxyPtzController::getLimits(
    Qn::PtzCoordinateSpace space,
    QnPtzLimits* limits) const
{
    return m_controller
        ? m_controller->getLimits(space, limits)
        : false;
}

bool QnProxyPtzController::getFlip(Qt::Orientations* flip) const
{
    return m_controller
        ? m_controller->getFlip(flip)
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

bool QnProxyPtzController::getTours(QnPtzTourList* tours) const
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

bool QnProxyPtzController::getAuxilaryTraits(QnPtzAuxilaryTraitList* auxilaryTraits) const
{
    return m_controller
        ? m_controller->getAuxilaryTraits(auxilaryTraits)
        : false;
}

bool QnProxyPtzController::runAuxilaryCommand(
    const QnPtzAuxilaryTrait& trait,
    const QString& data)
{
    return m_controller
        ? m_controller->runAuxilaryCommand(trait, data)
        : false;
}

bool QnProxyPtzController::getData(Qn::PtzDataFields query, QnPtzData* data) const
{
    // This is important because of base implementation! Do not change it!
    return base_type::getData(query, data);
}

void QnProxyPtzController::baseFinished(Qn::PtzCommand command, const QVariant& data)
{
    emit finished(command, data);
}

void QnProxyPtzController::baseChanged(Qn::PtzDataFields fields)
{
    emit changed(fields);
}
