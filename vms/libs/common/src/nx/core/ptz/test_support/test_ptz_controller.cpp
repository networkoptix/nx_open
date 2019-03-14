#include "test_ptz_controller.h"

namespace nx {
namespace core {
namespace ptz {
namespace test_support {

TestPtzController::TestPtzController():
    base_type(QnResourcePtr())
{
}

TestPtzController::TestPtzController(const QnResourcePtr& resource):
    base_type(resource)
{
}

Ptz::Capabilities TestPtzController::getCapabilities(const nx::core::ptz::Options& options) const
{
    if (m_getCapabilitiesExecutor)
        return m_getCapabilitiesExecutor(options);

    if (m_predefinedCapabilities != std::nullopt)
        return *m_predefinedCapabilities;

    return Ptz::NoPtzCapabilities;
}

bool TestPtzController::continuousMove(
    const nx::core::ptz::Vector& speed,
    const nx::core::ptz::Options& options)
{
    if (m_continuousMoveExecutor)
        return m_continuousMoveExecutor(speed, options);

    return false;
}

bool TestPtzController::continuousFocus(qreal speed, const nx::core::ptz::Options& options)
{
    if (m_continuousFocusExecutor)
        return m_continuousFocusExecutor(speed, options);

    return false;
}

bool TestPtzController::absoluteMove(
    Qn::PtzCoordinateSpace space,
    const nx::core::ptz::Vector& position,
    qreal speed,
    const nx::core::ptz::Options& options)
{
    if (m_absoluteMoveExecutor)
        return m_absoluteMoveExecutor(space, position, speed, options);

    return false;
}

bool TestPtzController::viewportMove(
    qreal aspectRatio,
    const QRectF& viewport,
    qreal speed,
    const nx::core::ptz::Options& options)
{
    if (m_viewportMoveExecutor)
        return m_viewportMoveExecutor(aspectRatio, viewport, speed, options);

    return false;
}

bool TestPtzController::relativeMove(
    const nx::core::ptz::Vector& direction,
    const nx::core::ptz::Options& options)
{
    if (m_relativeMoveExecutor)
        return m_relativeMoveExecutor(direction, options);

    return false;
}

bool TestPtzController::relativeFocus(qreal direction, const nx::core::ptz::Options& options)
{
    if (m_relativeFocusExecutor)
        return m_relativeFocusExecutor(direction, options);

    return false;
}

bool TestPtzController::getPosition(
    Qn::PtzCoordinateSpace space,
    nx::core::ptz::Vector* position,
    const nx::core::ptz::Options& options) const
{
    if (m_getPositionExecutor)
        return m_getPositionExecutor(space, position, options);

    if (m_predefinedPosition != std::nullopt)
    {
        *position = *m_predefinedPosition;
        return true;
    }

    return false;
}

bool TestPtzController::getLimits(
    Qn::PtzCoordinateSpace space,
    QnPtzLimits* limits,
    const nx::core::ptz::Options& options) const
{
    if (m_getLimitsExecutor)
        return m_getLimitsExecutor(space, limits, options);

    if (m_predefinedLimits != std::nullopt)
    {
        *limits = *m_predefinedLimits;
        return true;
    }

    return false;
}

bool TestPtzController::getFlip(
    Qt::Orientations* flip,
    const nx::core::ptz::Options& options) const
{
    if (m_getFlipExecutor)
        return m_getFlipExecutor(flip, options);

    if (m_predefinedFlip != std::nullopt)
    {
        *flip = *m_predefinedFlip;
        return true;
    }

    return false;
}

bool TestPtzController::createPreset(const QnPtzPreset& preset)
{
    if (m_createPresetExecutor)
        return m_createPresetExecutor(preset);

    return false;
}

bool TestPtzController::updatePreset(const QnPtzPreset& preset)
{
    if (m_updatePresetExecutor)
        return m_updatePresetExecutor(preset);

    return false;
}

bool TestPtzController::removePreset(const QString& presetId)
{
    if (m_removePresetExecutor)
        return m_removePresetExecutor(presetId);

    return false;
}

bool TestPtzController::activatePreset(const QString& presetId, qreal speed)
{
    if (m_activatePresetExecutor)
        return m_activatePresetExecutor(presetId, speed);

    return false;
}

bool TestPtzController::getPresets(QnPtzPresetList* presets) const
{
    if (m_getPresetsExecutor)
        return m_getPresetsExecutor(presets);

    if (m_predefinedPresets != std::nullopt)
    {
        *presets = *m_predefinedPresets;
        return true;
    }

    return false;
}

bool TestPtzController::createTour(const QnPtzTour& tour)
{
    if (m_createTourExecutor)
        return m_createTourExecutor(tour);

    return false;
}

bool TestPtzController::removeTour(const QString& tourId)
{
    if (m_removeTourExecutor)
        return m_removeTourExecutor(tourId);

    return false;
}

bool TestPtzController::activateTour(const QString& tourId)
{
    if (m_activateTourExecutor)
        return m_activateTourExecutor(tourId);

    return false;
}

bool TestPtzController::getTours(QnPtzTourList* tours) const
{
    if (m_getToursExecutor)
        return m_getToursExecutor(tours);

    if (m_predefinedTours != std::nullopt)
    {
        *tours = *m_predefinedTours;
        return true;
    }

    return false;
}


bool TestPtzController::getActiveObject(QnPtzObject* activeObject) const
{
    if (m_getActiveObjectExecutor)
        return m_getActiveObjectExecutor(activeObject);

    if (m_predefinedActiveObject != std::nullopt)
    {
        *activeObject = *m_predefinedActiveObject;
        return true;
    }

    return false;
}

bool TestPtzController::updateHomeObject(const QnPtzObject& homeObject)
{
    if (m_updateHomeObjectExecutor)
        return m_updateHomeObjectExecutor(homeObject);

    return false;
}

bool TestPtzController::getHomeObject(QnPtzObject* homeObject) const
{
    if (m_getHomeObjectExecutor)
        return m_getHomeObjectExecutor(homeObject);

    if (m_predefinedHomeObject != std::nullopt)
    {
        *homeObject = *m_predefinedHomeObject;
        return true;
    }

    return false;
}

bool TestPtzController::getAuxiliaryTraits(
    QnPtzAuxiliaryTraitList* auxiliaryTraits,
    const nx::core::ptz::Options& options) const
{
    if (m_getAuxiliaryTraitsExecutor)
        return m_getAuxiliaryTraitsExecutor(auxiliaryTraits, options);

    if (m_predefinedAuxiliaryTraits != std::nullopt)
    {
        *auxiliaryTraits = *m_predefinedAuxiliaryTraits;
        return true;
    }

    return false;
}

bool TestPtzController::runAuxiliaryCommand(
    const QnPtzAuxiliaryTrait& trait,
    const QString& data,
    const nx::core::ptz::Options& options)
{
    if (m_runAuxiliaryCommandExecutor)
        return m_runAuxiliaryCommandExecutor(trait, data, options);

    return false;
}

bool TestPtzController::getData(
    Qn::PtzDataFields query,
    QnPtzData* data,
    const nx::core::ptz::Options& options) const
{
    if (m_getDataExecutor)
        return m_getDataExecutor(query, data, options);

    if (m_predefinedData != std::nullopt)
    {
        *data = *m_predefinedData;
        return true;
    }

    return false;
}

void TestPtzController::setCapabilities(std::optional<Ptz::Capabilities> capabilities)
{
    m_predefinedCapabilities = std::move(capabilities);
}

void TestPtzController::setPosition(std::optional<nx::core::ptz::Vector> position)
{
    m_predefinedPosition = std::move(position);
}

void TestPtzController::setLimits(std::optional<QnPtzLimits> limits)
{
    m_predefinedLimits = std::move(limits);
}

void TestPtzController::setFlip(std::optional<Qt::Orientations> flip)
{
    m_predefinedFlip = std::move(flip);
}

void TestPtzController::setPresets(std::optional<QnPtzPresetList> presets)
{
    m_predefinedPresets = std::move(presets);
}

void TestPtzController::setTours(std::optional<QnPtzTourList> tours)
{
    m_predefinedTours = std::move(tours);
}

void TestPtzController::setActiveObject(std::optional<QnPtzObject> activeObject)
{
    m_predefinedActiveObject = std::move(activeObject);
}

void TestPtzController::setHomeObject(std::optional<QnPtzObject> activeObject)
{
    m_predefinedHomeObject = std::move(activeObject);
}

void TestPtzController::setAuxiliaryTraits(std::optional<QnPtzAuxiliaryTraitList> traits)
{
    m_predefinedAuxiliaryTraits = std::move(traits);
}

void TestPtzController::setData(std::optional<QnPtzData> data)
{
    m_predefinedData = std::move(data);
}

} // namespace test_support
} // namespace ptz
} // namespace core
} // namespace nx
