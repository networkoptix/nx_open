#include "workaround_ptz_controller.h"

#include <common/common_module.h>

#include <utils/math/math.h>
#include <utils/math/coordinate_transformations.h>
#include <nx/fusion/model_functions.h>

#include <core/resource/resource_data.h>
#include <core/resource_management/resource_data_pool.h>
#include <core/resource/camera_resource.h>

namespace core_ptz = nx::core::ptz;

namespace {

core_ptz::Override loadOverride(const QnVirtualCameraResourcePtr& camera)
{
    core_ptz::Override ptzOverride;
    const auto resourceData = camera->resourceData();
    resourceData.value<core_ptz::Override>(core_ptz::Override::kPtzOverrideKey, &ptzOverride);

    const std::map<QString, std::optional<Ptz::Capabilities>*> overrides = {
        {
            ResourceDataKey::kOperationalPtzCapabilities,
            &ptzOverride.operational.capabilitiesOverride
        },
        {
            ResourceDataKey::kConfigurationalPtzCapabilities,
            &ptzOverride.configurational.capabilitiesOverride
        }
    };

    for (const auto& entry: overrides)
    {
        Ptz::Capabilities capabilitiesOverride;
        const auto parameterName = entry.first;
        auto capabilities = entry.second;

        if (resourceData.value<Ptz::Capabilities>(parameterName, &capabilitiesOverride))
            *capabilities = capabilitiesOverride;
    }

    const auto userAddedPtzCapabilities = camera->ptzCapabilitiesAddedByUser();
    ptzOverride.operational.capabilitiesToAdd |= userAddedPtzCapabilities;

    return ptzOverride;
}

bool isContinuousMoveOverriden(const core_ptz::OverridePart& ptzOverride)
{
    return ptzOverride.flip != 0
        || (ptzOverride.traits & (Ptz::FourWayPtzTrait | Ptz::EightWayPtzTrait));
}

} // namespace

QnWorkaroundPtzController::QnWorkaroundPtzController(const QnPtzControllerPtr &baseController):
    base_type(baseController)
{
    QnVirtualCameraResourcePtr camera = resource().dynamicCast<QnVirtualCameraResource>();
    if(!camera)
        return;

    m_override = loadOverride(camera);
}

Ptz::Capabilities QnWorkaroundPtzController::getCapabilities(
    const nx::core::ptz::Options& options) const
{
    const auto ptzOverride = m_override.overrideByType(options.type);
    if (ptzOverride.capabilitiesOverride != std::nullopt)
        return *ptzOverride.capabilitiesOverride;

    return (base_type::getCapabilities(options) | ptzOverride.capabilitiesToAdd)
        & ~ptzOverride.capabilitiesToRemove;
}

bool QnWorkaroundPtzController::continuousMove(
    const nx::core::ptz::Vector& speed,
    const nx::core::ptz::Options& options)
{
    const auto ptzOverride = m_override.overrideByType(options.type);
    if(!isContinuousMoveOverriden(ptzOverride))
        return base_type::continuousMove(speed, options);

    auto localSpeed = speed;
    if(ptzOverride.flip & Qt::Horizontal)
        localSpeed.pan = localSpeed.pan * -1;
    if(ptzOverride.flip & Qt::Vertical)
        localSpeed.tilt = localSpeed.tilt * -1;

    if(ptzOverride.traits & (Ptz::EightWayPtzTrait | Ptz::FourWayPtzTrait))
    {
        // Rounding is 45 or 90 degrees.
        float rounding = (ptzOverride.traits & Ptz::EightWayPtzTrait) ? M_PI / 4.0 : M_PI / 2.0;

        QVector2D cartesianSpeed = localSpeed.toQVector2D();
        QnPolarPoint<float> polarSpeed = cartesianToPolar(cartesianSpeed);
        polarSpeed.alpha = qRound(polarSpeed.alpha, rounding);
        cartesianSpeed = polarToCartesian<QVector2D>(polarSpeed.r, polarSpeed.alpha);

        if(qFuzzyIsNull(cartesianSpeed.x())) //< TODO: #Elric use lower null threshold
            cartesianSpeed.setX(0.0);
        if(qFuzzyIsNull(cartesianSpeed.y()))
            cartesianSpeed.setY(0.0);

        localSpeed = nx::core::ptz::Vector(
            cartesianSpeed.x(),
            cartesianSpeed.y(),
            localSpeed.rotation,
            localSpeed.zoom);
    }

    return base_type::continuousMove(localSpeed, options);
}

bool QnWorkaroundPtzController::extends(Ptz::Capabilities)
{
    // TODO: #Elric if no workaround is needed for a camera, we don't really have to extend.
    return true;
}
