#include "basic_ptz_controller.h"

QnBasicPtzController::QnBasicPtzController(const QnResourcePtr& resource):
    base_type(resource)
{
}

Ptz::Capabilities QnBasicPtzController::getCapabilities(
    const nx::core::ptz::Options& /*options*/) const
{
    return Ptz::NoPtzCapabilities;
}

bool QnBasicPtzController::continuousMove(
    const nx::core::ptz::Vector& /*speed*/,
    const nx::core::ptz::Options& /*options*/)
{
    return false;
}

bool QnBasicPtzController::continuousFocus(
    qreal /*speed*/,
    const nx::core::ptz::Options& /*options*/)
{
    return false;
}

bool QnBasicPtzController::absoluteMove(
    Qn::PtzCoordinateSpace /*space*/,
    const nx::core::ptz::Vector& /*position*/,
    qreal /*speed*/,
    const nx::core::ptz::Options& /*options*/)
{
    return false;
}

bool QnBasicPtzController::relativeMove(
    const nx::core::ptz::Vector& /*direction*/,
    const nx::core::ptz::Options& /*options*/)
{
    return false;
}

bool QnBasicPtzController::relativeFocus(
    qreal /*direction*/,
    const nx::core::ptz::Options& /*options*/)
{
    return false;
}

bool QnBasicPtzController::viewportMove(
    qreal /*ratio*/,
    const QRectF& /*viewport*/,
    qreal /*speed*/,
    const nx::core::ptz::Options& /*options*/)
{
    return false;
}

bool QnBasicPtzController::getPosition(
    Qn::PtzCoordinateSpace /*space*/,
    nx::core::ptz::Vector* /*position*/,
    const nx::core::ptz::Options& /*options*/) const
{
    return false;
}

bool QnBasicPtzController::getLimits(
    Qn::PtzCoordinateSpace /*space*/,
    QnPtzLimits* /*limits*/,
    const nx::core::ptz::Options& /*options*/) const
{
    return false;
}

bool QnBasicPtzController::getFlip(
    Qt::Orientations* /*flip*/,
    const nx::core::ptz::Options& /*options*/) const
{
    return false;
}

bool QnBasicPtzController::createPreset(const QnPtzPreset& /*preset*/)
{
    return false;
}

bool QnBasicPtzController::updatePreset(const QnPtzPreset& /*preset*/)
{
    return false;
}

bool QnBasicPtzController::removePreset(const QString& /*presetId*/)
{
    return false;
}

bool QnBasicPtzController::activatePreset(const QString& /*presetId*/, qreal /*speed*/)
{
    return false;
}

bool QnBasicPtzController::getPresets(QnPtzPresetList* /*presets*/) const
{
    return false;
}

bool QnBasicPtzController::createTour(const QnPtzTour& /*tour*/)
{
    return false;
}

bool QnBasicPtzController::removeTour(const QString& /*tourId*/)
{
    return false;
}

bool QnBasicPtzController::activateTour(const QString& /*tourId*/)
{
    return false;
}

bool QnBasicPtzController::getTours(QnPtzTourList* /*tours*/) const
{
    return false;
}

bool QnBasicPtzController::getActiveObject(QnPtzObject* /*object*/) const
{
    return false;
}

bool QnBasicPtzController::updateHomeObject(const QnPtzObject& /*object*/)
{
    return false;
}

bool QnBasicPtzController::getHomeObject(QnPtzObject* /*object*/) const
{
    return false;
}

bool QnBasicPtzController::getAuxiliaryTraits(
    QnPtzAuxiliaryTraitList* /*traits*/,
    const nx::core::ptz::Options& /*options*/) const
{
    return false;
}

bool QnBasicPtzController::runAuxiliaryCommand(
    const QnPtzAuxiliaryTrait& /*trait*/,
    const QString& /*data*/,
    const nx::core::ptz::Options& /*options*/)
{
    return false;
}

