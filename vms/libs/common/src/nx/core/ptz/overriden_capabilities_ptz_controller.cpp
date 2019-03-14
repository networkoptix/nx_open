#include "overriden_capabilities_ptz_controller.h"

namespace nx {
namespace core {
namespace ptz {

OverridenCapabilitiesPtzController::OverridenCapabilitiesPtzController(
    const QnPtzControllerPtr& controller,
    Ptz::Capabilities overridenCapabilities)
    :
    base_type(controller),
    m_overridenCapabilities(overridenCapabilities)
{
}

Ptz::Capabilities OverridenCapabilitiesPtzController::getCapabilities(
    const Options& options) const
{
    return m_overridenCapabilities;
}

bool OverridenCapabilitiesPtzController::continuousMove(
    const Vector& speed,
    const Options& options)
{
    if ((m_overridenCapabilities & Ptz::ContinuousPtrzCapabilities) == Ptz::NoPtzCapabilities)
        return false;

    return base_type::continuousMove(speed, options);
}

bool OverridenCapabilitiesPtzController::continuousFocus(qreal speed, const Options& options)
{
    if (!m_overridenCapabilities.testFlag(Ptz::ContinuousFocusCapability))
        return false;

    return base_type::continuousFocus(speed, options);
}

bool OverridenCapabilitiesPtzController::absoluteMove(
    Qn::PtzCoordinateSpace space,
    const Vector& position,
    qreal speed,
    const Options& options)
{
    if ((m_overridenCapabilities & Ptz::AbsolutePtrzCapabilities) == Ptz::NoPtzCapabilities)
        return false;

    return base_type::absoluteMove(space, position, speed, options);
}

bool OverridenCapabilitiesPtzController::viewportMove(
    qreal aspectRatio,
    const QRectF& viewport,
    qreal speed,
    const Options& options)
{
    if (!m_overridenCapabilities.testFlag(Ptz::ViewportPtzCapability))
        return false;

    return base_type::viewportMove(aspectRatio, viewport, speed, options);
}

bool OverridenCapabilitiesPtzController::relativeMove(
    const Vector& direction,
    const Options& options)
{
    if ((m_overridenCapabilities & Ptz::RelativePtrzCapabilities) == Ptz::NoPtzCapabilities)
        return false;

    return base_type::relativeMove(direction, options);
}

bool OverridenCapabilitiesPtzController::relativeFocus(qreal direction, const Options& options)
{
    if (!m_overridenCapabilities.testFlag(Ptz::RelativeFocusCapability))
        return false;

    return base_type::relativeFocus(direction, options);
}

bool OverridenCapabilitiesPtzController::getPosition(
    Qn::PtzCoordinateSpace space,
    Vector* position,
    const Options& options) const
{
    if ((m_overridenCapabilities & Ptz::AbsolutePtrzCapabilities) == Ptz::NoPtzCapabilities)
        return false;

    return base_type::getPosition(space, position, options);
}

bool OverridenCapabilitiesPtzController::getLimits(
    Qn::PtzCoordinateSpace space,
    QnPtzLimits* limits,
    const Options& options) const
{
    if (!m_overridenCapabilities.testFlag(Ptz::LimitsPtzCapability))
        return false;

    return base_type::getLimits(space, limits, options);
}

bool OverridenCapabilitiesPtzController::getFlip(
    Qt::Orientations* flip,
    const Options& options) const
{
    if (!m_overridenCapabilities.testFlag(Ptz::FlipPtzCapability))
        return false;

    return base_type::getFlip(flip, options);
}

bool OverridenCapabilitiesPtzController::createPreset(const QnPtzPreset& preset)
{
    if (!m_overridenCapabilities.testFlag(Ptz::PresetsPtzCapability))
        return false;

    return base_type::createPreset(preset);
}

bool OverridenCapabilitiesPtzController::updatePreset(const QnPtzPreset& preset)
{
    if (!m_overridenCapabilities.testFlag(Ptz::PresetsPtzCapability))
        return false;

    return base_type::updatePreset(preset);
}

bool OverridenCapabilitiesPtzController::removePreset(const QString& presetId)
{
    if (!m_overridenCapabilities.testFlag(Ptz::PresetsPtzCapability))
        return false;

    return base_type::removePreset(presetId);
}

bool OverridenCapabilitiesPtzController::activatePreset(const QString& presetId, qreal speed)
{
    if (!m_overridenCapabilities.testFlag(Ptz::PresetsPtzCapability))
        return false;

    return base_type::activatePreset(presetId, speed);
}

bool OverridenCapabilitiesPtzController::getPresets(QnPtzPresetList* presets) const
{
    if (!m_overridenCapabilities.testFlag(Ptz::PresetsPtzCapability))
        return false;

    return base_type::getPresets(presets);
}

bool OverridenCapabilitiesPtzController::createTour(const QnPtzTour& tour)
{
    if (!m_overridenCapabilities.testFlag(Ptz::ToursPtzCapability))
        return false;

    return base_type::createTour(tour);
}

bool OverridenCapabilitiesPtzController::removeTour(const QString& tourId)
{
    if (!m_overridenCapabilities.testFlag(Ptz::ToursPtzCapability))
        return false;

    return base_type::removeTour(tourId);
}

bool OverridenCapabilitiesPtzController::activateTour(const QString& tourId)
{
    if (!m_overridenCapabilities.testFlag(Ptz::ToursPtzCapability))
        return false;

    return base_type::activateTour(tourId);
}

bool OverridenCapabilitiesPtzController::getTours(QnPtzTourList* tours) const
{
    if (!m_overridenCapabilities.testFlag(Ptz::ToursPtzCapability))
        return false;

    return base_type::getTours(tours);
}

bool OverridenCapabilitiesPtzController::getActiveObject(QnPtzObject* activeObject) const
{
    if (!m_overridenCapabilities.testFlag(Ptz::ActivityPtzCapability))
        return false;

    return base_type::getActiveObject(activeObject);
}

bool OverridenCapabilitiesPtzController::updateHomeObject(const QnPtzObject& homeObject)
{
    if (!m_overridenCapabilities.testFlag(Ptz::HomePtzCapability))
        return false;

    return base_type::updateHomeObject(homeObject);
}

bool OverridenCapabilitiesPtzController::getHomeObject(QnPtzObject* homeObject) const
{
    if (!m_overridenCapabilities.testFlag(Ptz::HomePtzCapability))
        return false;

    return base_type::getHomeObject(homeObject);
}

bool OverridenCapabilitiesPtzController::getAuxiliaryTraits(
    QnPtzAuxiliaryTraitList* auxiliaryTraits,
    const Options& options) const
{
    if (!m_overridenCapabilities.testFlag(Ptz::AuxiliaryPtzCapability))
        return false;

    return base_type::getAuxiliaryTraits(auxiliaryTraits, options);
}

bool OverridenCapabilitiesPtzController::runAuxiliaryCommand(
    const QnPtzAuxiliaryTrait& trait,
    const QString& data,
    const Options& options)
{
    if (!m_overridenCapabilities.testFlag(Ptz::AuxiliaryPtzCapability))
        return false;

    return base_type::runAuxiliaryCommand(trait, data, options);
}

bool OverridenCapabilitiesPtzController::getData(
    Qn::PtzDataFields query,
    QnPtzData* data,
    const Options& options) const
{
    const bool result = base_type::getData(query, data, options);
    data->capabilities = m_overridenCapabilities;

    return result;
}

} // namespace ptz
} // namespace core
} // namespace nx
