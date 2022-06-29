// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "fallback_ptz_controller.h"

#include <cassert>

#include <core/resource/resource.h>

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
    NX_ASSERT(
        mainController->resource() == fallbackController->resource(),
        "Fallback controller was created with two different resources ('%1' != '%2').",
        mainController->resource()->getName(),
        fallbackController->resource()->getName());

    connect(mainController.get(), &QnAbstractPtzController::finished,
        this, &QnFallbackPtzController::baseFinished);
    connect(fallbackController.get(), &QnAbstractPtzController::finished,
        this, &QnFallbackPtzController::baseFinished);
    connect(mainController.get(), &QnAbstractPtzController::changed,
        this, &QnFallbackPtzController::baseChanged);
    connect(fallbackController.get(), &QnAbstractPtzController::changed,
        this, &QnFallbackPtzController::baseChanged);

    baseChanged(DataField::capabilities);
}

QnFallbackPtzController::~QnFallbackPtzController()
{
}

void QnFallbackPtzController::baseChanged(DataFields fields)
{
    if (fields.testFlag(DataField::capabilities))
    {
        m_hasOperationalCapabilities = m_mainController
            ->getCapabilities({Type::operational}) != Ptz::NoPtzCapabilities;

        m_hasConfigurationalCapabilities = m_mainController
            ->getCapabilities({Type::configurational}) != Ptz::NoPtzCapabilities;
    }

    emit changed(fields);
}

const QnPtzControllerPtr& QnFallbackPtzController::baseController(
    const Options& options) const
{
    const bool isControllerValid = options.type == Type::operational
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

Ptz::Capabilities QnFallbackPtzController::getCapabilities(const Options& options) const
{
    return baseController(options)->getCapabilities(options);
}

bool QnFallbackPtzController::continuousMove(
    const Vector& speed,
    const Options& options)
{
    return baseController(options)->continuousMove(speed, options);
}

bool QnFallbackPtzController::continuousFocus(
    qreal speed,
    const Options& options)
{
    return baseController(options)->continuousFocus(speed, options);
}

bool QnFallbackPtzController::absoluteMove(
    CoordinateSpace space,
    const Vector& position,
    qreal speed,
    const Options& options)
{
    return baseController(options)->absoluteMove(space, position, speed, options);
}

bool QnFallbackPtzController::viewportMove(
    qreal aspectRatio,
    const QRectF& viewport,
    qreal speed,
    const Options& options)
{
    return baseController(options)->viewportMove(aspectRatio, viewport, speed, options);
}

bool QnFallbackPtzController::relativeMove(
    const Vector& direction,
    const Options& options)
{
    return baseController(options)->relativeMove(direction, options);
}

bool QnFallbackPtzController::relativeFocus(qreal direction, const Options& options)
{
    return baseController(options)->relativeFocus(direction, options);
}

bool QnFallbackPtzController::getPosition(
    Vector* position,
    CoordinateSpace space,
    const Options& options) const
{
    return baseController(options)->getPosition(position, space, options);
}

bool QnFallbackPtzController::getLimits(
    QnPtzLimits* limits,
    CoordinateSpace space,
    const Options& options) const
{
    return baseController(options)->getLimits(limits, space, options);
}

bool QnFallbackPtzController::getFlip(
    Qt::Orientations* flip,
    const Options& options) const
{
    return baseController(options)->getFlip(flip, options);
}

bool QnFallbackPtzController::createPreset(const QnPtzPreset& preset)
{
    return baseController({Type::operational})->createPreset(preset);
}
bool QnFallbackPtzController::updatePreset(const QnPtzPreset& preset)
{
    return baseController({Type::operational})->updatePreset(preset);
}

bool QnFallbackPtzController::removePreset(const QString& presetId)
{
    return baseController({Type::operational})->removePreset(presetId);
}

bool QnFallbackPtzController::activatePreset(const QString& presetId, qreal speed)
{
    return baseController({Type::operational})->activatePreset(presetId, speed);
}

bool QnFallbackPtzController::getPresets(QnPtzPresetList* presets) const
{
    return baseController({Type::operational})->getPresets(presets);
}

bool QnFallbackPtzController::createTour(const QnPtzTour& tour)
{
    return baseController({Type::operational})->createTour(tour);
}

bool QnFallbackPtzController::removeTour(const QString& tourId)
{
    return baseController({Type::operational})->removeTour(tourId);
}

bool QnFallbackPtzController::activateTour(const QString& tourId)
{
    return baseController({Type::operational})->activateTour(tourId);
}

bool QnFallbackPtzController::getTours(QnPtzTourList* tours) const
{
    return baseController({Type::operational})->getTours(tours);
}

bool QnFallbackPtzController::getActiveObject(QnPtzObject* activeObject) const
{
    return baseController({Type::operational})->getActiveObject(activeObject);
}

bool QnFallbackPtzController::updateHomeObject(
    const QnPtzObject& homeObject)
{
    return baseController({Type::operational})->updateHomeObject(homeObject);
}

bool QnFallbackPtzController::getHomeObject(QnPtzObject* homeObject) const
{
    return baseController({Type::operational})->getHomeObject(homeObject);
}

bool QnFallbackPtzController::getAuxiliaryTraits(
    QnPtzAuxiliaryTraitList* auxiliaryTraits,
    const Options& options) const
{
    return baseController(options)->getAuxiliaryTraits(auxiliaryTraits, options);
}

bool QnFallbackPtzController::runAuxiliaryCommand(
    const QnPtzAuxiliaryTrait& trait,
    const QString& data,
    const Options& options)
{
    return baseController(options)->runAuxiliaryCommand(trait, data, options);
}

bool QnFallbackPtzController::getData(
    QnPtzData* data,
    DataFields query,
    const Options& options) const
{
    return baseController(options)->getData(data, query, options);
}

void QnFallbackPtzController::baseFinished(Command command, const QVariant& data)
{
    emit finished(command, data);
}
