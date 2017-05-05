#include "fallback_ptz_controller.h"

#include <cassert>

#include <core/resource/resource.h>
#include <utils/common/warnings.h>

QnFallbackPtzController::QnFallbackPtzController(
    const QnPtzControllerPtr& mainController,
    const QnPtzControllerPtr& fallbackController)
    :
    base_type(mainController->resource()),
    m_mainIsValid(true),
    m_mainController(mainController),
    m_fallbackController(fallbackController)
{
    if(mainController->resource() != fallbackController->resource())
    {
        qnWarning("Fallback controller was created with two different resources ('%1' != '%2').",
            mainController->resource()->getName(), fallbackController->resource()->getName());
    }

    connect(mainController, &QnAbstractPtzController::finished,
        this, &QnFallbackPtzController::baseFinished);
    connect(fallbackController, &QnAbstractPtzController::finished,
        this, &QnFallbackPtzController::baseFinished);
    connect(mainController, &QnAbstractPtzController::changed,
        this, &QnFallbackPtzController::baseChanged);
    connect(fallbackController, &QnAbstractPtzController::changed,
        this, &QnFallbackPtzController::baseChanged);

    baseChanged(Qn::CapabilitiesPtzField);
}

QnFallbackPtzController::~QnFallbackPtzController()
{
}

void QnFallbackPtzController::baseChanged(Qn::PtzDataFields fields)
{
    if(fields.testFlag(Qn::CapabilitiesPtzField))
        m_mainIsValid = m_mainController->getCapabilities() != Ptz::NoPtzCapabilities;

    emit changed(fields);
}

const QnPtzControllerPtr& QnFallbackPtzController::baseController() const
{
    return m_mainIsValid ? m_mainController : m_fallbackController;
}

QnPtzControllerPtr QnFallbackPtzController::mainController() const

{
    return m_mainController;
}

QnPtzControllerPtr QnFallbackPtzController::fallbackController() const
{
    return m_fallbackController;
}

Ptz::Capabilities QnFallbackPtzController::getCapabilities() const
{
    return baseController()->getCapabilities();
}

bool QnFallbackPtzController::continuousMove(const QVector3D& speed)
{
    return baseController()->continuousMove(speed);
}

bool QnFallbackPtzController::continuousFocus(qreal speed)
{
    return baseController()->continuousFocus(speed);
}

bool QnFallbackPtzController::absoluteMove(
    Qn::PtzCoordinateSpace space,
    const QVector3D& position,
    qreal speed)
{
    return baseController()->absoluteMove(space, position, speed);
}

bool QnFallbackPtzController::viewportMove(
    qreal aspectRatio,
    const QRectF& viewport,
    qreal speed)
{
    return baseController()->viewportMove(aspectRatio, viewport, speed);
}

bool QnFallbackPtzController::getPosition(Qn::PtzCoordinateSpace space, QVector3D* position) const
{
    return baseController()->getPosition(space, position);
}

bool QnFallbackPtzController::getLimits(Qn::PtzCoordinateSpace space, QnPtzLimits* limits) const
{
    return baseController()->getLimits(space, limits);
}

bool QnFallbackPtzController::getFlip(Qt::Orientations* flip) const
{
    return baseController()->getFlip(flip);
}

bool QnFallbackPtzController::createPreset(const QnPtzPreset& preset)
{
    return baseController()->createPreset(preset);
}
bool QnFallbackPtzController::updatePreset(const QnPtzPreset& preset)
{
    return baseController()->updatePreset(preset);
}

bool QnFallbackPtzController::removePreset(const QString& presetId)
{
    return baseController()->removePreset(presetId);
}

bool QnFallbackPtzController::activatePreset(const QString& presetId, qreal speed)
{
    return baseController()->activatePreset(presetId, speed);
}

bool QnFallbackPtzController::getPresets(QnPtzPresetList* presets) const
{
    return baseController()->getPresets(presets);
}

bool QnFallbackPtzController::createTour(const QnPtzTour& tour)
{
    return baseController()->createTour(tour);
}

bool QnFallbackPtzController::removeTour(const QString& tourId)
{
    return baseController()->removeTour(tourId);
}

bool QnFallbackPtzController::activateTour(const QString& tourId)
{
    return baseController()->activateTour(tourId);
}

bool QnFallbackPtzController::getTours(QnPtzTourList* tours) const
{
    return baseController()->getTours(tours);
}

bool QnFallbackPtzController::getActiveObject(QnPtzObject* activeObject) const
{
    return baseController()->getActiveObject(activeObject);
}

bool QnFallbackPtzController::updateHomeObject(const QnPtzObject& homeObject)
{
    return baseController()->updateHomeObject(homeObject);
}

bool QnFallbackPtzController::getHomeObject(QnPtzObject* homeObject) const
{
    return baseController()->getHomeObject(homeObject);
}

bool QnFallbackPtzController::getAuxilaryTraits(QnPtzAuxilaryTraitList* auxilaryTraits) const
{
    return baseController()->getAuxilaryTraits(auxilaryTraits);
}

bool QnFallbackPtzController::runAuxilaryCommand(const QnPtzAuxilaryTrait& trait, const QString& data)
{
    return baseController()->runAuxilaryCommand(trait, data);
}

bool QnFallbackPtzController::getData(Qn::PtzDataFields query, QnPtzData* data) const
{
    return baseController()->getData(query, data);
}

void QnFallbackPtzController::baseFinished(Qn::PtzCommand command, const QVariant& data)
{
    emit finished(command, data);
}
