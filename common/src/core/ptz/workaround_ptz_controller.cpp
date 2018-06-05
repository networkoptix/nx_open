#include "workaround_ptz_controller.h"

#include <common/common_module.h>

#include <utils/math/math.h>
#include <utils/math/coordinate_transformations.h>
#include <nx/fusion/serialization/lexical_functions.h>

#include <core/resource/resource_data.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_data_pool.h>
#include <common/static_common_module.h>

QnWorkaroundPtzController::QnWorkaroundPtzController(const QnPtzControllerPtr &baseController):
    base_type(baseController),
    m_overrideContinuousMove(false),
    m_flip(0),
    m_traits(Ptz::NoPtzTraits),
    m_overrideCapabilities(false),
    m_capabilities(Ptz::NoPtzCapabilities)
{
    QnVirtualCameraResourcePtr camera = resource().dynamicCast<QnVirtualCameraResource>();
    if(!camera)
        return;

    QnResourceData resourceData = qnStaticCommon->dataPool()->data(camera);

    resourceData.value(lit("ptzTraits"), &m_traits);

    if(resourceData.value<bool>(lit("panFlipped"), false))
        m_flip |= Qt::Horizontal;
    if(resourceData.value<bool>(lit("tiltFlipped"), false))
        m_flip |= Qt::Vertical;

    m_overrideContinuousMove = m_flip != 0 || (m_traits & (Ptz::FourWayPtzTrait | Ptz::EightWayPtzTrait));

    resourceData.value(Qn::PTZ_CAPABILITIES_TO_ADD_PARAM_NAME, &m_capabilitiesToAdd);
    resourceData.value(Qn::PTZ_CAPABILITIES_TO_REMOVE_PARAM_NAME, &m_capabilitiesToRemove);

    if(resourceData.value(Qn::PTZ_CAPABILITIES_PARAM_NAME, &m_capabilities))
        m_overrideCapabilities = true;
}

Ptz::Capabilities QnWorkaroundPtzController::getCapabilities() const
{
    if (m_overrideCapabilities)
        return m_capabilities;

    return (base_type::getCapabilities() | m_capabilitiesToAdd) & ~m_capabilitiesToRemove;
}

bool QnWorkaroundPtzController::continuousMove(const nx::core::ptz::PtzVector& speed)
{
    if(!m_overrideContinuousMove)
        return base_type::continuousMove(speed);

    auto localSpeed = speed;
    if(m_flip & Qt::Horizontal)
        localSpeed.pan = localSpeed.pan * -1;
    if(m_flip & Qt::Vertical)
        localSpeed.tilt = localSpeed.tilt * -1;

    if(m_traits & (Ptz::EightWayPtzTrait | Ptz::FourWayPtzTrait))
    {
        float rounding = (m_traits & Ptz::EightWayPtzTrait) ? M_PI / 4.0 : M_PI / 2.0; /* 45 or 90 degrees. */

        QVector2D cartesianSpeed = localSpeed.toQVector2D();
        QnPolarPoint<float> polarSpeed = cartesianToPolar(cartesianSpeed);
        polarSpeed.alpha = qRound(polarSpeed.alpha, rounding);
        cartesianSpeed = polarToCartesian<QVector2D>(polarSpeed.r, polarSpeed.alpha);

        if(qFuzzyIsNull(cartesianSpeed.x())) // TODO: #Elric use lower null threshold
            cartesianSpeed.setX(0.0);
        if(qFuzzyIsNull(cartesianSpeed.y()))
            cartesianSpeed.setY(0.0);

        localSpeed = nx::core::ptz::PtzVector(
            cartesianSpeed.x(),
            cartesianSpeed.y(),
            localSpeed.rotation,
            localSpeed.zoom);
    }

    return base_type::continuousMove(localSpeed);
}

bool QnWorkaroundPtzController::extends(Ptz::Capabilities) {
    return true; // TODO: #Elric if no workaround is needed for a camera, we don't really have to extend.
}

