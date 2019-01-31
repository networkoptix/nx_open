#include "fallback_ptz_controller.h"

#include <cassert>

#include <core/resource/resource.h>
#include <utils/common/warnings.h>

using namespace nx::core;

QnFallbackPtzController::QnFallbackPtzController(
    const QnPtzControllerPtr& mainController,
    const QnPtzControllerPtr& fallbackController)
    :
    base_type(mainController->resource()),
    m_hasOperationalCapabilities(true),
    m_hasConfigurationalCapabilities(true),
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
    if (fields.testFlag(Qn::CapabilitiesPtzField))
    {
        m_hasOperationalCapabilities = m_mainController
            ->getCapabilities({ptz::Type::operational}) != Ptz::NoPtzCapabilities;

        m_hasConfigurationalCapabilities = m_mainController
            ->getCapabilities({ptz::Type::configurational}) != Ptz::NoPtzCapabilities;
    }

    emit changed(fields);
}

const QnPtzControllerPtr& QnFallbackPtzController::baseController(
    const nx::core::ptz::Options& options) const
{
    const bool isControllerValid = options.type == ptz::Type::operational
        ? m_hasOperationalCapabilities
        : m_hasConfigurationalCapabilities;

    return isControllerValid ? m_mainController : m_fallbackController;
}

QnPtzControllerPtr QnFallbackPtzController::mainController() const
{
    return m_mainController;
}

QnPtzControllerPtr QnFallbackPtzController::fallbackController() const
{
    return m_fallbackController;
}

Ptz::Capabilities QnFallbackPtzController::getCapabilities(const nx::core::ptz::Options& options) const
{
    return baseController(options)->getCapabilities(options);
}

bool QnFallbackPtzController::continuousMove(
    const nx::core::ptz::Vector& speed,
    const nx::core::ptz::Options& options)
{
    return baseController(options)->continuousMove(speed, options);
}

bool QnFallbackPtzController::continuousFocus(
    qreal speed,
    const nx::core::ptz::Options& options)
{
    return baseController(options)->continuousFocus(speed, options);
}

bool QnFallbackPtzController::absoluteMove(
    Qn::PtzCoordinateSpace space,
    const nx::core::ptz::Vector& position,
    qreal speed,
    const nx::core::ptz::Options& options)
{
    return baseController(options)->absoluteMove(space, position, speed, options);
}

bool QnFallbackPtzController::viewportMove(
    qreal aspectRatio,
    const QRectF& viewport,
    qreal speed,
    const nx::core::ptz::Options& options)
{
    return baseController(options)->viewportMove(aspectRatio, viewport, speed, options);
}

bool QnFallbackPtzController::relativeMove(
    const ptz::Vector& direction,
    const ptz::Options& options)
{
    return baseController(options)->relativeMove(direction, options);
}

bool QnFallbackPtzController::relativeFocus(qreal direction, const ptz::Options& options)
{
    return baseController(options)->relativeFocus(direction, options);
}

bool QnFallbackPtzController::getPosition(
    Qn::PtzCoordinateSpace space,
    nx::core::ptz::Vector* position,
    const nx::core::ptz::Options& options) const
{
    return baseController(options)->getPosition(space, position, options);
}

bool QnFallbackPtzController::getLimits(
    Qn::PtzCoordinateSpace space,
    QnPtzLimits* limits,
    const nx::core::ptz::Options& options) const
{
    return baseController(options)->getLimits(space, limits, options);
}

bool QnFallbackPtzController::getFlip(
    Qt::Orientations* flip,
    const nx::core::ptz::Options& options) const
{
    return baseController(options)->getFlip(flip, options);
}

bool QnFallbackPtzController::createPreset(const QnPtzPreset& preset)
{
    return baseController({nx::core::ptz::Type::operational})->createPreset(preset);
}
bool QnFallbackPtzController::updatePreset(const QnPtzPreset& preset)
{
    return baseController({nx::core::ptz::Type::operational})->updatePreset(preset);
}

bool QnFallbackPtzController::removePreset(const QString& presetId)
{
    return baseController({nx::core::ptz::Type::operational})->removePreset(presetId);
}

bool QnFallbackPtzController::activatePreset(const QString& presetId, qreal speed)
{
    return baseController({nx::core::ptz::Type::operational})->activatePreset(presetId, speed);
}

bool QnFallbackPtzController::getPresets(QnPtzPresetList* presets) const
{
    return baseController({nx::core::ptz::Type::operational})->getPresets(presets);
}

bool QnFallbackPtzController::createTour(const QnPtzTour& tour)
{
    return baseController({nx::core::ptz::Type::operational})->createTour(tour);
}

bool QnFallbackPtzController::removeTour(const QString& tourId)
{
    return baseController({nx::core::ptz::Type::operational})->removeTour(tourId);
}

bool QnFallbackPtzController::activateTour(const QString& tourId)
{
    return baseController({nx::core::ptz::Type::operational})->activateTour(tourId);
}

bool QnFallbackPtzController::getTours(QnPtzTourList* tours) const
{
    return baseController({nx::core::ptz::Type::operational})->getTours(tours);
}

bool QnFallbackPtzController::getActiveObject(QnPtzObject* activeObject) const
{
    return baseController({nx::core::ptz::Type::operational})->getActiveObject(activeObject);
}

bool QnFallbackPtzController::updateHomeObject(
    const QnPtzObject& homeObject)
{
    return baseController({nx::core::ptz::Type::operational})->updateHomeObject(homeObject);
}

bool QnFallbackPtzController::getHomeObject(QnPtzObject* homeObject) const
{
    return baseController({nx::core::ptz::Type::operational})->getHomeObject(homeObject);
}

bool QnFallbackPtzController::getAuxiliaryTraits(
    QnPtzAuxiliaryTraitList* auxiliaryTraits,
    const nx::core::ptz::Options& options) const
{
    return baseController(options)->getAuxiliaryTraits(auxiliaryTraits, options);
}

bool QnFallbackPtzController::runAuxiliaryCommand(
    const QnPtzAuxiliaryTrait& trait,
    const QString& data,
    const nx::core::ptz::Options& options)
{
    return baseController(options)->runAuxiliaryCommand(trait, data, options);
}

bool QnFallbackPtzController::getData(
    Qn::PtzDataFields query,
    QnPtzData* data,
    const nx::core::ptz::Options& options) const
{
    return baseController(options)->getData(query, data, options);
}

void QnFallbackPtzController::baseFinished(Qn::PtzCommand command, const QVariant& data)
{
    emit finished(command, data);
}
